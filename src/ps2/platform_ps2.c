/* The PS2 platform (src/pc/platform/platform.h): the GS shows the PS1 GPU's
 * picture, the DualShock is the PS1 pad, audsrv plays the SPU's mix, and the
 * game's files are on the USB stick (or host: in an emulator). It stands in
 * for src/pc/platform/sdl.c; the frame clock is platform_common.c's
 * cooperative one.
 *
 * Startup (main, below): reset the IOP, load the drivers, find the disc
 * image beside the ELF, then the port's own main (src/pc/guest/main.c,
 * built as Memories_PcMain). */
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <audsrv.h>
extern int fileXioInit(void); /* newlib then goes through fileXio (iomanX devices: mass:) */
#include <gsKit.h>
#include <dmaKit.h>
#include <debug.h>
#include <stdarg.h>
#include <malloc.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "pc/platform/platform.h"
#include "pc/platform/controls_runtime.h"
#include "pc/platform/menu.h"
#include "pc/platform/loading.h"
#include "pc/cards/cards.h"
#include "ps2.h"
#include "pc/saves/save_menu.h"
#include "pc/saves/deck_menu.h"
#include "gs_gpu.h"
#include "ajustes.h"
#include <graph.h>

extern int Memories_PcMain(int argc, char **argv);
extern void Cheats_Frame(void);
extern void Duelists_Frame(void);
extern void Tower_Frame(void);

/* ---- the on-screen log (stdout too: an emulator's console) ---- */
#define LOG_LINES 8
static char log_lines[LOG_LINES][64];
static unsigned log_next;

void Ps2_Log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(log_lines[log_next % LOG_LINES], sizeof(log_lines[0]), format, args);
    va_end(args);
    printf("ps2: %s\n", log_lines[log_next % LOG_LINES]);
    log_next++;
}

static int video_open;
static void video_close(void);

/* ---- the boot log: on screen until the game's picture takes over, and in
 * YFM-LOG.txt beside the ELF (closed and reopened at each step, so what it
 * says survives a hang: the card's file size is only written at a close). */
static char log_path[300];
static char program_dir[256];
static int boot_screen;
void Ps2_LogSync(void)
{
    if (!log_path[0]) return;
    fflush(stdout);
    if (!freopen(log_path, "a", stdout)) log_path[0] = 0;
    else setvbuf(stdout, NULL, _IOLBF, 0);
}
static char quiet_buffer[16384], quiet_path[300];
void Ps2_LogQuiet(void)
{
    if (!log_path[0]) return;
    fflush(stdout);
    if (!freopen(log_path, "a", stdout)) {
        log_path[0] = 0;
        return;
    }
    setvbuf(stdout, quiet_buffer, _IOFBF, sizeof(quiet_buffer));
    snprintf(quiet_path, sizeof(quiet_path), "%s", log_path);
    log_path[0] = 0; /* no more syncs, but Ps2_Fatal's flush and Ps2_LogSave */
}
/* The log written out and its file closed now (so the stick has it), at a
 * moment the game is not reading the disc: the end of a movie. */
void Ps2_LogSave(void)
{
    if (!quiet_path[0]) return;
    fflush(stdout);
    if (freopen(quiet_path, "a", stdout)) setvbuf(stdout, quiet_buffer, _IOFBF, sizeof(quiet_buffer));
    else quiet_path[0] = 0;
}
void Ps2_Stage(const char *format, ...)
{
    char line[160];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    printf("ps2: stage: %s\n", line);
    if (boot_screen) scr_printf("  %s\n", line);
    Ps2_LogSync();
}

void Ps2_Fatal(const char *message)
{
    unsigned i;
    printf("ps2: FATAL: %s\n", message);
    fflush(stdout);
    Ps2_LogSync();
    fclose(stdout); /* the stick's file gets its size at the close */
    video_close();
    init_scr();
    scr_printf("\n\n  Yu-Gi-Oh! Forbidden Memories - PS2\n\n  %s\n\n", message);
    for (i = log_next > LOG_LINES ? log_next - LOG_LINES : 0; i < log_next; i++)
        scr_printf("  %s\n", log_lines[i % LOG_LINES]);
    for (;;) SleepThread();
}

/* ---- IOP modules ---- */
#define IRX(name) extern unsigned char irx_##name[], irx_##name##_end[]
IRX(iomanx); IRX(filexio); IRX(usbd); IRX(bdm); IRX(bdmfs); IRX(usbmass); IRX(libsd); IRX(audsrv); IRX(sio2man);
IRX(padman); IRX(cdfs); IRX(mcman); IRX(mcserv);

static int load_buffer(const char *name, unsigned char *start, unsigned char *end)
{
    int result = 0, id = SifExecModuleBuffer(start, (unsigned)(end - start), 0, NULL, &result);
    if (id < 0 || result == 1) {
        printf("ps2: module %s: %d (%d)\n", name, id, result);
        return -1;
    }
    return 0;
}

static int pad_ok, audio_ok, usb_ok, mc_ok, cd_ok;
/* O ELF veio de um disco (uma ISO no OPL ou no PCSX2, um CD gravado): o jogo
 * é lido do próprio disco pelo cdrom0: da BIOS (YFM.BIN), e os saves e a
 * configuração vão para o cartão de memória. O USB não é carregado: no OPL
 * ele é do próprio OPL, que lê a ISO do pendrive. */
static int from_disc;
/* disc_mode: 1 com from_disc; 2 no teste do cartão no emulador (host: com
 * mctest.txt). Os saves vão para mc0:/YFMPS2 (ou mc1:) nos dois. */
static int disc_mode;
static unsigned char pad_buffer[256] __attribute__((aligned(64)));

/* Boot por ISO (OPL no modo ISO, PCSX2, CD gravado): como o port de Super
 * Mario World faz. Nada de iomanX/fileXio/cdfs nem USB: no OPL o USB é do
 * próprio OPL, e o IOP dele é montado para os módulos da BIOS, como nos
 * jogos comerciais. O newlib fica no fio (IOMAN da BIOS), que lê o disco
 * por cdrom0: e o cartão por mc0:. Os módulos do SDK embutidos só entram
 * se a BIOS não tiver os X. */
static void iop_start_disc(void)
{
    if (SifLoadModule("rom0:XSIO2MAN", 0, NULL) >= 0) {
        pad_ok = SifLoadModule("rom0:XPADMAN", 0, NULL) >= 0 ? 2 : !load_buffer("padman", irx_padman, irx_padman_end);
        mc_ok = SifLoadModule("rom0:XMCMAN", 0, NULL) >= 0;
        if (!mc_ok) { /* o XMCMAN precisa do SECRMAN */
            SifLoadModule("rom0:SECRMAN", 0, NULL);
            mc_ok = SifLoadModule("rom0:XMCMAN", 0, NULL) >= 0;
        }
        mc_ok = mc_ok && SifLoadModule("rom0:XMCSERV", 0, NULL) >= 0;
    } else {
        pad_ok = !load_buffer("sio2man", irx_sio2man, irx_sio2man_end) && !load_buffer("padman", irx_padman, irx_padman_end);
        mc_ok = !load_buffer("mcman", irx_mcman, irx_mcman_end);
        if (!mc_ok) {
            SifLoadModule("rom0:SECRMAN", 0, NULL);
            mc_ok = !load_buffer("mcman", irx_mcman, irx_mcman_end);
        }
        mc_ok = mc_ok && !load_buffer("mcserv", irx_mcserv, irx_mcserv_end);
    }
    if (pad_ok && (padInit(0) < 0 || padPortOpen(0, 0, pad_buffer) <= 0)) pad_ok = 0;
    audio_ok = (SifLoadModule("rom0:LIBSD", 0, NULL) >= 0 || !load_buffer("libsd", irx_libsd, irx_libsd_end)) &&
               !load_buffer("audsrv", irx_audsrv, irx_audsrv_end) && audsrv_init() == 0;
    printf("ps2: IOP (disco) ready (pad %d, audio %d, mc %d)\n", pad_ok, audio_ok, mc_ok);
}

static void iop_start(void)
{
    SifInitRpc(0);
    while (!SifIopReset("", 0)) {}
    while (!SifIopSync()) {}
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    sbv_patch_fileio();
    if (from_disc) {
        iop_start_disc();
        return;
    }
    if (!load_buffer("iomanX", irx_iomanx, irx_iomanx_end) && !load_buffer("fileXio", irx_filexio, irx_filexio_end))
        fileXioInit();
    if (disc_mode) {
        cd_ok = !load_buffer("cdfs", irx_cdfs, irx_cdfs_end);
    } else {
        usb_ok = !load_buffer("usbd", irx_usbd, irx_usbd_end) && !load_buffer("bdm", irx_bdm, irx_bdm_end) &&
                 !load_buffer("bdmfs", irx_bdmfs, irx_bdmfs_end) && !load_buffer("usbmass", irx_usbmass, irx_usbmass_end);
    }
    /* The pad driver the SDK's libpad speaks to: its own, as the SDK built
     * it; the console's newer ROM one if that cannot load; the old ROM one
     * last (the 0.1 prototype found consoles where one or the other worked). */
    if (!load_buffer("sio2man", irx_sio2man, irx_sio2man_end) && !load_buffer("padman", irx_padman, irx_padman_end))
        pad_ok = 1;
    else if (SifLoadModule("rom0:XSIO2MAN", 0, NULL) >= 0 && SifLoadModule("rom0:XPADMAN", 0, NULL) >= 0)
        pad_ok = 2;
    else if (SifLoadModule("rom0:SIO2MAN", 0, NULL) >= 0 && SifLoadModule("rom0:PADMAN", 0, NULL) >= 0)
        pad_ok = 3;
    if (pad_ok && (padInit(0) < 0 || padPortOpen(0, 0, pad_buffer) <= 0)) pad_ok = 0;
    mc_ok = !load_buffer("mcman", irx_mcman, irx_mcman_end) && !load_buffer("mcserv", irx_mcserv, irx_mcserv_end);
    audio_ok = !load_buffer("libsd", irx_libsd, irx_libsd_end) && !load_buffer("audsrv", irx_audsrv, irx_audsrv_end) &&
               audsrv_init() == 0;
    printf("ps2: IOP ready (usb %d, pad %d, audio %d, cd %d, mc %d)\n", usb_ok, pad_ok, audio_ok, cd_ok, mc_ok);
}

/* ---- video ---- */
static GSGLOBAL *gs;
static GSTEXTURE picture;
static uint16_t *picture16;
static uint32_t *picture32;
#define PICTURE_W 640
#define PICTURE_H 512

static void video_start(void)
{
    if (video_open) return;
    gs = gsKit_init_global();
    gs->Mode = GS_MODE_NTSC;
    gs->Interlace = GS_INTERLACED;
    gs->Field = GS_FIELD;
    gs->Width = 640;
    gs->Height = 448;
    gs->PSM = GS_PSM_CT24;
    gs->PSMZ = GS_PSMZ_16S;
    gs->ZBuffering = GS_SETTING_OFF;
    gs->DoubleBuffering = GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    picture16 = memalign(128, PICTURE_W * PICTURE_H * 2);
    picture32 = memalign(128, PICTURE_W * PICTURE_H * 4);
    picture.Width = PICTURE_W;
    picture.Height = PICTURE_H;
    picture.PSM = GS_PSM_CT32;
    picture.Filter = GS_FILTER_LINEAR;
    picture.Vram = gsKit_vram_alloc(gs, gsKit_texture_size(PICTURE_W, PICTURE_H, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
    video_open = 1;
}

static void video_close(void)
{
    if (!video_open) return;
    video_open = 0;
}

static void show(int w, int h, int psm, void *pixels)
{
    picture.Width = w;
    picture.Height = h;
    picture.PSM = psm;
    picture.Mem = pixels;
    gsKit_texture_upload(gs, &picture);
    gsKit_clear(gs, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
    gsKit_prim_sprite_texture(gs, &picture, 0.0f, 0.0f, 0.0f, 0.0f, (float)gs->Width, (float)gs->Height,
                              (float)w, (float)h, 1, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
    gsKit_queue_exec(gs);
    gsKit_sync_flip(gs);
}

void Platform_Present(const uint16_t *vram, int stride, int x, int y, int w, int h, int rgb24)
{
    int i, j;
    static unsigned presents;
    if (GsGpu_Active()) {
        if (++presents == 1) Ps2_Stage("primeira imagem %dx%d%s", w, h, rgb24 ? " (24-bit)" : "");
        if (presents == 120) {
            Ps2_Stage("120 quadros ok");
            /* Booted: from here the log is kept in memory and written in
             * 16 KiB pieces, never in the middle of a movie's reads. */
            Ps2_LogQuiet();
        }
        GsGpu_Present(vram, x, y, w, h, rgb24);
        return;
    }
    if (!video_open || w <= 0 || h <= 0) return;
    if (w > PICTURE_W) w = PICTURE_W;
    if (h > PICTURE_H) h = PICTURE_H;
    if (rgb24) {
        for (j = 0; j < h; j++) {
            const uint8_t *bytes = (const uint8_t *)(vram + ((y + j) & 511) * stride + x);
            uint32_t *out = picture32 + j * w;
            for (i = 0; i < w; i++, bytes += 3) out[i] = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | 0x80000000u;
        }
        FlushCache(0);
        show(w, h, GS_PSM_CT32, picture32);
    } else {
        for (j = 0; j < h; j++) {
            const uint16_t *row = vram + ((y + j) & 511) * stride;
            uint16_t *out = picture16 + j * w;
            if (x + w <= 1024) memcpy(out, row + x, (size_t)w * 2);
            else for (i = 0; i < w; i++) out[i] = row[(x + i) & 1023];
            for (i = 0; i < w; i++) out[i] |= 0x8000u;
        }
        FlushCache(0);
        show(w, h, GS_PSM_CT16, picture16);
    }
    if (++presents == 1) printf("ps2: first picture %dx%d%s\n", w, h, rgb24 ? " (24-bit)" : "");
}

int Platform_PresentPicture(const uint32_t *pixels, int stride, int x, int y, int w, int h, int scale)
{
    (void)pixels; (void)stride; (void)x; (void)y; (void)w; (void)h; (void)scale;
    return 0;
}
int Platform_ReadPicture(uint32_t *out, int x, int y, int w, int h) { (void)out; (void)x; (void)y; (void)w; (void)h; return 0; }
int Platform_PresentWidePicture(int x, int y, int w, int h, int wide_w, int scale)
{
    (void)x; (void)y; (void)w; (void)h; (void)wide_w; (void)scale;
    return 0;
}
int Platform_ReadWidePicture(uint32_t *out, int x, int y, int w, int h, int wide_w, int scale)
{
    (void)out; (void)x; (void)y; (void)w; (void)h; (void)wide_w; (void)scale;
    return 0;
}
int Platform_Widescreen(void) { return 0; }
void Platform_SetScale(int scale) { (void)scale; }
void Platform_ApplyDisplaySettings(void) {}
int Platform_HasWindowModes(void) { return 0; }
void Platform_Screenshot(int window_image) { (void)window_image; }
int Platform_CopyText(const char *text) { (void)text; return 0; }

/* ---- the window's other duties ---- */
static const LoadingSink loading_sink = {Platform_ShowLoading, Platform_EndLoading, NULL};

int Platform_Open(const char *title)
{
    (void)title;
    ControlsRuntime_Init();
    Menu_LoadSettingsWithoutMods();
    Menu_LoadMods();
    Ps2_Stage("video: GS");
    boot_screen = 0;
    if (getenv("PS2_SOFT_PICTURE")) video_start();
    else GsGpu_Init();
    Ps2_Stage("video ligado");
    Loading_Begin(&loading_sink);
    return 0;
}

int Platform_ShowLoading(const char *line, unsigned done, unsigned total)
{
    static char last[64];
    if (line && strncmp(last, line, sizeof(last) - 1)) {
        snprintf(last, sizeof(last), "%s", line);
        printf("ps2: loading: %s\n", line);
    }
    (void)done; (void)total;
    return -1;
}
void Platform_EndLoading(void) {}

void Platform_ShowError(const char *title, const char *message)
{
    char text[512];
    snprintf(text, sizeof(text), "%s:\n  %s", title ? title : "Error", message ? message : "");
    Ps2_Fatal(text);
}

int Platform_SelectDisc(char *path, size_t size, char *why, size_t why_size)
{
    (void)path; (void)size;
    if (why && why_size && !why[0]) snprintf(why, why_size, "no disc image");
    return -1;
}

void Platform_OpenMods(void) {}
void Platform_OpenControls(void) {}
int Platform_OpenFolder(const char *path) { (void)path; return -1; }
int Platform_OpenUrl(const char *url) { (void)url; return -1; }
static volatile int quit_asked;
int Platform_ShouldQuit(void) { return quit_asked; }
void Platform_RequestQuit(void) { quit_asked = 1; }
static int state_slot = 1;
int Platform_StateSlot(void) { return state_slot; }
void Platform_SetStateSlot(int slot) { state_slot = slot; }
int Platform_WindowPointer(int *x, int *y, unsigned *buttons) { (void)x; (void)y; (void)buttons; return 0; }
void Platform_PumpEvents(void) {}

/* ---- the pad ---- */
static volatile uint16_t pad_bits;

static void poll_pad(void)
{
    struct padButtonStatus status;
    int state;
    if (!pad_ok) return;
    state = padGetState(0, 0);
    if ((state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) && padRead(0, 0, &status) > 0)
        pad_bits = (uint16_t)(0xFFFFu ^ status.btns);
}

/* ---- AJUSTES (ajustes.c, como no port de Super Mario World) ----
 * YFM-VISUAL.txt ao lado do ELF (na ISO, no cartão): cinco dígitos, a
 * suavização (0 desligada, 1 média, 2 forte), a tela (0 4:3, 1 16:9), o
 * vídeo (0 240p, 1 480p; o 2, 480i, saiu e vira 240p), o idioma (0 português, 1 inglês) e,
 * desde 20/11/2026, o tamanho na TV (0 = 100%, cada número 3% menor; ver ajustes.h). Com o vídeo salvo, a tela AJUSTES começa
 * sozinha em 5 s. ---- */
static int ajustes_salvos; /* o arquivo existia, com o vídeo */
/* O idioma: 0 português (languages/pt-br.txt), 1 inglês (o original). */
static int idioma, sem_traducao;

/* Onde está a tradução: ao lado do ELF (pendrive) ou na raiz da ISO, como
 * PTBR.TXT (a ISO só aceita nomes de 8 letras, sem hífen). */
static void caminho_traducao(char *path, size_t size)
{
    if (from_disc) {
        /* Na raiz (a página criar-port) ou em LANG/ (tools/ps2/iso/montar_iso). */
        FILE *file = fopen("cdrom0:\\PTBR.TXT;1", "rb");
        if (file) fclose(file);
        snprintf(path, size, "%s", file ? "cdrom0:\\PTBR.TXT;1" : "cdrom0:\\LANG\\PTBR.TXT;1");
    }
    else if (!strncmp(program_dir, "host", 4)) snprintf(path, size, "host:languages/pt-br.txt");
    else snprintf(path, size, "%s/languages/pt-br.txt", program_dir);
}

/* O idioma para o código do jogo (language.c), antes de o texto ser montado. */
static void aplica_idioma(void)
{
    char path[320];
    FILE *file;
    caminho_traducao(path, sizeof(path));
    sem_traducao = !(file = fopen(path, "rb"));
    if (file) fclose(file);
    if (sem_traducao) idioma = 1;
    setenv("MEMORIES_LANGUAGE", idioma ? "0" : "6", 1); /* LANGUAGE_US ou LANGUAGE_PT_BR */
    if (!idioma) setenv("MEMORIES_LANGUAGE_FILE", path, 1);
    else unsetenv("MEMORIES_LANGUAGE_FILE");
    printf("ps2: idioma %s (%s)\n", idioma ? "ingles" : "portugues", sem_traducao ? "sem traducao" : path);
}
/* O primeiro cartão de memória (slot 1, depois o 2) em que dá para gravar na
 * pasta YFMPS2; sem nenhum, o do slot 1. Os saves e o YFM-VISUAL.txt usam o
 * mesmo cartão (antes a configuração ia sempre para o slot 1). */
static int cartao_gravavel(void)
{
    int slot;
    for (slot = 0; slot < 2; slot++) {
        char dir[32], probe_path[48];
        FILE *probe;
        snprintf(dir, sizeof(dir), "mc%d:/YFMPS2", slot);
        mkdir(dir, 0777);
        snprintf(probe_path, sizeof(probe_path), "%s/PROBE.TXT", dir);
        probe = fopen(probe_path, "wb");
        printf("ps2: memory card %d: %s (errno %d)\n", slot, probe ? "ok" : "sem acesso", probe ? 0 : errno);
        if (probe) {
            fclose(probe);
            remove(probe_path);
            return slot;
        }
    }
    return 0;
}

static void visual_path(char *path, size_t size)
{
    if (!strncmp(program_dir, "host", 4)) snprintf(path, size, "host:YFM-VISUAL.txt");
    else snprintf(path, size, "%s/YFM-VISUAL.txt", program_dir);
}

static void load_visual(void)
{
    char path[320];
    FILE *file;
    int c;
    visual_path(path, sizeof(path));
    if ((file = fopen(path, "rb"))) {
        c = fgetc(file);
        if (c >= '0' && c <= '2') GsGpu_SetVisual(c - '0');
        c = fgetc(file);
        if (c == '0' || c == '1') GsGpu_SetWide(c - '0');
        c = fgetc(file);
        /* 1 = 480p; 0 = 240p; 2 (o 480i das versões anteriores, que saiu) vira 240p. */
        if (c >= '0' && c <= '2') GsGpu_SetVideo(c == '1' ? VIDEO_480P : VIDEO_240P), ajustes_salvos = 1;
        c = fgetc(file);
        if (c == '0' || c == '1') idioma = c - '0';
        c = fgetc(file);
        if (c >= '0' && c < '0' + AJUSTE_TAMANHOS) GsGpu_SetTamanho(AJUSTE_PORCENTO(c - '0'));
        fclose(file);
    }
    printf("ps2: suavizacao %d, 16:9 %d, video %d\n", GsGpu_Visual(), GsGpu_Wide(), GsGpu_Video());
}

static void save_visual(void)
{
    char path[320];
    FILE *file;
    visual_path(path, sizeof(path));
    if ((file = fopen(path, "wb"))) {
        fputc('0' + GsGpu_Visual(), file);
        fputc('0' + GsGpu_Wide(), file);
        fputc('0' + GsGpu_Video(), file);
        fputc('0' + idioma, file);
        fputc('0' + (100 - GsGpu_Tamanho()) / 3, file);
        fputc('\n', file);
        fclose(file);
    }
}

/* ---- as telas AJUSTES e "A imagem apareceu?" ---- */
extern unsigned char msx[]; /* a fonte 8x8 do libdebug do PS2SDK */
static uint32_t *quadro; /* só durante os AJUSTES: a memória do port é apertada */

static uint16_t botoes(void)
{
    poll_pad();
    return pad_bits;
}

static void mostra(void)
{
    graph_wait_vsync();
    GsGpu_ShowPicture(quadro);
}

/* No modo novo: X ou START em 10 s confirma que a TV mostra a imagem. */
static int confirma_video(void)
{
    uint16_t held = botoes();
    int frame;
    for (frame = 0; frame < 10 * 60; ++frame) {
        uint16_t agora = botoes(), apertou = agora & ~held;
        held = agora;
        if (frame > 20 && (apertou & (PAD_CROSS | PAD_START))) return 1;
        ajustes_confirma(quadro, msx, GsGpu_Video(), 10 - frame / 60, frame);
        mostra();
    }
    return 0;
}

/* AJUSTES: a tela aparece em 240p (toda TV mostra, pelo AV também), e o modo escolhido
 * vale para o jogo. Com tudo salvo, começa sozinha em 5 s, até apertarem
 * um botão. Um modo de vídeo novo pergunta se a imagem apareceu. */
static void tela_ajustes(void)
{
    static char situacao[64], aviso_texto[64];
    const int salvo_video = GsGpu_Video(), salvo_tela = GsGpu_Wide(), salvo_suave = GsGpu_Visual(), salvo_idioma = idioma,
              salvo_tamanho = GsGpu_Tamanho();
    int contagem = ajustes_salvos;
    const char *aviso = NULL;
    aplica_idioma(); /* vê se a tradução está lá */
    if (!pad_ok) return; /* sem controle não há o que escolher */
    if (!(quadro = memalign(64, AJUSTES_W * AJUSTES_H * 4))) return;
    boot_screen = 0;     /* daqui em diante o GS é do port, não da tela de texto */
    GsGpu_Init();
    if (from_disc) snprintf(situacao, sizeof(situacao), mc_ok ? "Saves: Memory Card (YFMPS2)" : "Sem Memory Card: os saves não ficam.");
    else snprintf(situacao, sizeof(situacao), "Saves: pendrive, ao lado do jogo");
    for (;;) {
        Ajustes a = {contagem ? AJUSTE_JOGAR : AJUSTE_VIDEO, GsGpu_Video(), GsGpu_Wide(), GsGpu_Visual(), idioma,
                     sem_traducao, -1, aviso, situacao, (100 - GsGpu_Tamanho()) / 3};
        uint16_t held = botoes();
        int frame, saindo = -1;
        GsGpu_SetVideo(VIDEO_240P);
        for (frame = 0;; ++frame) {
            uint16_t agora = botoes(), apertou = agora & ~held;
            held = agora;
            if (saindo < 0 && apertou) {
                int dir = (apertou & PAD_LEFT) ? -1 : (apertou & PAD_RIGHT) ? 1 : 0;
                contagem = 0;
                if (apertou & (PAD_CROSS | PAD_START)) {
                    saindo = frame;
                } else if (apertou & (PAD_UP | PAD_DOWN)) {
                    a.linha = (a.linha + ((apertou & PAD_UP) ? AJUSTE_LINHAS - 1 : 1)) % AJUSTE_LINHAS;
                } else if (dir && a.linha != AJUSTE_JOGAR) {
                    /* 240p e 480p. O 480i saiu (20/11/2026): pelo AV o 240p
                     * fica melhor; o 480p é para componente ou adaptador HDMI. */
                    if (a.linha == AJUSTE_VIDEO) a.video = a.video == VIDEO_480P ? VIDEO_240P : VIDEO_480P;
                    if (a.linha == AJUSTE_TELA) a.tela = !a.tela;
                    if (a.linha == AJUSTE_SUAVE) a.suave = (a.suave + 3 + dir) % 3;
                    if (a.linha == AJUSTE_IDIOMA && !a.sem_traducao) a.idioma = !a.idioma;
                    if (a.linha == AJUSTE_TAMANHO) {
                        /* Muda na hora: a própria tela AJUSTES é a prévia. */
                        a.tamanho += dir;
                        a.tamanho = a.tamanho < 0 ? 0 : a.tamanho >= AJUSTE_TAMANHOS ? AJUSTE_TAMANHOS - 1 : a.tamanho;
                        GsGpu_SetTamanho(AJUSTE_PORCENTO(a.tamanho));
                    }
                    a.aviso = NULL;
                }
            }
            if (saindo < 0 && contagem && frame >= 5 * 60) saindo = frame;
            a.segundos = contagem ? 5 - frame / 60 : -1;
            ajustes_desenha(quadro, msx, &a, frame);
            if (saindo >= 0 && frame - saindo > 8) break;
            mostra();
        }
        contagem = 0;
        aviso = NULL;
        GsGpu_SetWide(a.tela);
        GsGpu_SetVisual(a.suave);
        GsGpu_SetVideo(a.video);
        idioma = a.idioma;
        /* O modo já confirmado nesta TV entra direto; um novo pergunta. */
        if (ajustes_salvos && a.video == salvo_video) break;
        if (confirma_video()) break;
        {
            static const char *const nomes[] = {"240p", "480p", "480i"};
            snprintf(aviso_texto, sizeof(aviso_texto), "A TV não mostrou o %s. Escolha outro.", nomes[a.video]);
            aviso = aviso_texto;
            GsGpu_SetVideo(ajustes_salvos ? salvo_video : VIDEO_240P);
        }
    }
    if (!ajustes_salvos || GsGpu_Video() != salvo_video || GsGpu_Wide() != salvo_tela || GsGpu_Visual() != salvo_suave ||
        idioma != salvo_idioma || GsGpu_Tamanho() != salvo_tamanho)
        save_visual();
    aplica_idioma();
    printf("ps2: ajustes: video %d, 16:9 %d, suavizacao %d, tamanho %d%%\n", GsGpu_Video(), GsGpu_Wide(), GsGpu_Visual(),
           GsGpu_Tamanho());
    ajustes_carregando(quadro, msx, 0);
    mostra();
    free(quadro);
    quadro = NULL;
}

static int test_slot_ok(unsigned char *state) { (void)state; return 1; }
static unsigned current_frame;
/* Emulator runs (host:): a command file the developer writes while the game
 * runs, "<id>\n<frames>:<bits hex>,..." -- each step holds the pad bits for
 * that many frames, from when a new id is seen. */
static int live_input;
static uint16_t live_bits;
static struct { unsigned frames; uint16_t bits; } live_steps[256];
static unsigned live_count, live_at, live_left;

static void poll_live_input(void)
{
    static char last_id[32];
    char text[4096], *at;
    FILE *file;
    size_t got;
    if (!live_input || current_frame % 20) return;
    if (!(file = fopen("host:cmd.txt", "rb"))) return;
    got = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[got] = '\0';
    at = strchr(text, '\n');
    if (!at) return;
    *at++ = '\0';
    if (!strcmp(text, last_id)) return;
    snprintf(last_id, sizeof(last_id), "%s", text);
    live_count = live_at = live_left = 0;
    while (*at && live_count < 256) {
        char *end;
        unsigned long frames = strtoul(at, &end, 10);
        if (*end != ':') break;
        live_steps[live_count].frames = (unsigned)frames;
        live_steps[live_count].bits = (uint16_t)strtoul(end + 1, &end, 16);
        live_count++;
        at = *end == ',' ? end + 1 : end;
        while (*at == '\n' || *at == '\r' || *at == ' ') at++;
    }
    printf("ps2: live input %s: %u steps at frame %u\n", last_id, live_count, current_frame);
}

static void step_live_input(void)
{
    while (live_at < live_count && !live_left) {
        live_bits = live_steps[live_at].bits;
        live_left = live_steps[live_at].frames;
        live_at++;
    }
    if (live_left) live_left--;
    else live_bits = 0;
}

/* Com L2 e R2 apertados juntos o controle é dos atalhos de vídeo (como no
 * port do Mario): o jogo não vê nada. */
uint16_t Platform_Pad(int port)
{
    if (port == 0 && (pad_bits & (PAD_L2 | PAD_R2)) == (PAD_L2 | PAD_R2)) return 0;
    return port == 0 ? (uint16_t)(pad_bits | live_bits | ControlsRuntime_Keyboard() | Platform_ScriptedBits(current_frame))
                     : 0;
}
uint16_t Platform_PadFixedBits(int port) { (void)port; return 0; }
int Platform_PadConnected(int port) { return port == 0; }

/* ---- audio: the SPU's mix (22.05 kHz stereo) through audsrv, on the game's
 * thread at each frame: the mixer reads what the game's sound driver wrote,
 * which only changes on this thread too. ---- */
static void (*mixer)(int16_t *frames, size_t count);
static int16_t mix_buffer[2048 * 2] __attribute__((aligned(64)));
static unsigned underruns;

static uint64_t now_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000u + (uint64_t)now.tv_nsec / 1000u;
}

/* The mix follows the clock: each call mixes the samples the time since the
 * last one calls for (22050 a second), and the first adds AUDIO_LEAD samples
 * (about 0.1 s) that stay queued ahead of the player, so a long frame (a
 * movie's decode) does not drain it. It is called at each frame, from the
 * clock's ticks, the waits and the movie decoder (Ps2_PumpAudio), on the
 * game's thread: the mixer reads what the game's sound driver writes there.
 * audsrv's queued/available counts are not reliable on every IOP image, so
 * only the free room bounds a call (more is dropped, not queued late). */
#define AUDIO_LEAD 2205
static uint64_t audio_clock_us, audio_last_pump, audio_longest_gap;
void Ps2_PumpAudio(void)
{
    int room;
    uint64_t now;
    size_t frames;
    static int pumping;
    if (!mixer || !audio_ok || pumping) return;
    now = now_us();
    if (audio_last_pump && now - audio_last_pump > audio_longest_gap) audio_longest_gap = now - audio_last_pump;
    audio_last_pump = now;
    if (!audio_clock_us || now - audio_clock_us > 300000u) {
        /* The start, or a stall (a load): from now, with the lead again. */
        audio_clock_us = now;
        frames = AUDIO_LEAD;
    } else {
        frames = (size_t)((now - audio_clock_us) * 22050u / 1000000u);
        if (frames < 64) return;
        audio_clock_us += (uint64_t)frames * 1000000u / 22050u;
    }
    room = audsrv_available() / 4;
    if ((int)frames > room) {
        frames = room > 0 ? (size_t)room : 0;
        underruns++;
    }
    pumping = 1;
    while (frames) {
        size_t part = frames > 2048 ? 2048 : frames;
        mixer(mix_buffer, part);
        audsrv_play_audio((const char *)mix_buffer, (int)(part * 4));
        frames -= part;
    }
    pumping = 0;
}
static void pump_audio(void) { Ps2_PumpAudio(); }

/* Movie numbers for YFM-LOG.txt (guest_ps2.c's disc reads, libpress.c's
 * decode): every 30 frames of a 24-bit picture, and the log saved at its end. */
unsigned long long Ps2_ReadUs, Ps2_MdecUs;
void Ps2_MovieFrame(int rgb24)
{
    static unsigned frames, total;
    static uint64_t since;
    uint64_t now = now_us();
    if (rgb24) {
        if (!frames++) since = now, Ps2_ReadUs = Ps2_MdecUs = 0, audio_longest_gap = 0;
        total++;
        if (frames == 30) {
            unsigned elapsed = (unsigned)(now - since);
            printf("ps2: video %u quadros: %u ms em 30 (%u fps), disco %u ms, mdec %u ms, maior pausa do audio %u ms, "
                   "audio cortado %u vezes\n", total, elapsed / 1000, elapsed ? 30000000u / elapsed : 0,
                   (unsigned)(Ps2_ReadUs / 1000), (unsigned)(Ps2_MdecUs / 1000), (unsigned)(audio_longest_gap / 1000),
                   underruns);
            frames = 0;
        }
    } else if (total) {
        printf("ps2: video terminou (%u quadros)\n", total);
        total = frames = 0;
        Ps2_LogSave();
    }
}

int Platform_StartAudio(void (*mix)(int16_t *frames, size_t count))
{
    audsrv_fmt_t format = {22050, 16, 2}; /* spu.c mixes at half the SPU's rate on the PS2 */
    if (!audio_ok || audsrv_set_format(&format)) {
        printf("ps2: no audio\n");
        return -1;
    }
    audsrv_set_volume(MAX_VOLUME);
    mixer = mix;
    return 0;
}

void Platform_AudioStats(int *queued_frames, unsigned *underrun_count)
{
    if (queued_frames) *queued_frames = 0;
    if (underrun_count) *underrun_count = underruns;
}

/* Once a presented frame (libgpu.c). */
void Platform_Frame(unsigned frame)
{
    current_frame = frame;
    poll_pad();
    poll_live_input();
    step_live_input();
    pump_audio();
    Cheats_Frame();
    Cards_Frame();
    Duelists_Frame();
    Tower_Frame();
    /* Anything that left supervisor mode (an SDK call that entered and left
     * kernel mode) is put back before the game touches its memory again. */
    Ps2_EnterGuestMode();
    if (frame == 1 || frame % 600 == 0) printf("ps2: frame %u\n", frame);
    {
        /* The game's accesses to PS1 physical addresses that the fault
         * handler served (sampler_ps2.c), each place once. */
        extern volatile int retail_reported;
        extern unsigned Ps2_RetailSites(const uint32_t **sites);
        static unsigned printed;
        if (retail_reported) {
            const uint32_t *sites;
            unsigned count = Ps2_RetailSites(&sites);
            retail_reported = 0;
            for (; printed < count; printed++) printf("ps2: retail access served at pc 0x%08x\n", (unsigned)sites[printed]);
        }
    }
    {
        extern int Ps2_ModelCalls;
        static int model_dumped, model_frames;
        if (live_input && Ps2_ModelCalls && !model_dumped && ++model_frames == 40) {
            model_dumped = 1;
            printf("ps2: the models ran (%d calls): dump\n", Ps2_ModelCalls);
            live_bits = 0xFFFF;
        }
    }
    {
        /* Host debugging: the save menu on its own, driven by the pad. */
        static unsigned char test_buffer[0x4000];
        static int testing;
        static uint16_t was;
        int sound;
        if (live_input && live_bits == 0xFFFD) {
            live_bits = 0;
            testing = 1;
            memset(test_buffer, 0, sizeof(test_buffer));
            SaveMenu_Begin(SAVE_MENU_SAVE, test_buffer + 0x200, NULL, 0x2000, "BASLUS-01411YUGIOH", test_slot_ok);
            SaveMenu_Poll(0, 0, &sound, test_slot_ok);
        } else if (testing && SaveMenu_Active()) {
            uint16_t now = live_bits, edge = now & ~was;
            unsigned pressed = (edge & 0x4000 ? 0x40u : 0) | (edge & 0x2000 ? 0x20u : 0) | (edge & 0x0040 ? 0x4000u : 0) |
                               (edge & 0x0010 ? 0x1000u : 0) | (edge & 0x0020 ? 0x2000u : 0) | (edge & 0x0080 ? 0x8000u : 0);
            was = now;
            SaveMenu_Poll(pressed, 0, &sound, test_slot_ok);
        } else {
            testing = 0;
        }
    }
    if (live_bits == 0xFFFC && live_input) {
        /* Depuração no PCSX2: a imagem exibida em host:shot.bin (largura e
         * altura em 16 bits, depois os pixels CT16). */
        uint16_t *shot = memalign(64, 448 * 256 * 2);
        int sw, sh;
        FILE *file;
        live_bits = 0;
        if (shot && GsGpu_ReadShown(shot, &sw, &sh) && (file = fopen("host:shot.bin", "wb"))) {
            uint16_t size[2] = {(uint16_t)sw, (uint16_t)sh};
            fwrite(size, 2, 2, file);
            fwrite(shot, 2, (size_t)(sw * sh), file);
            fclose(file);
            printf("ps2: shot %dx%d at frame %u\n", sw, sh, frame);
        }
        free(shot);
    }
    if (live_bits == 0xFFFE) {
        extern int GsGpu_FrameLog;
        live_bits = 0;
        GsGpu_FrameLog = 240;
    }
    {
        /* L1 + R1 + L2 + R2 + SELECT held: the GS's and the CPU's VRAM to
         * YFM-GSVRAM.bin and YFM-CPUVRAM.bin beside the ELF (a picture the
         * console draws differently from an emulator, for the developer). */
        static int held;
        int combo = (pad_bits & 0x0F01) == 0x0F01;
        if (combo && !held) live_bits = 0xFFFF;
        held = combo;
    }
    {
        /* Atalhos de vídeo, como no port do Mario: L2 + R2 + X troca 4:3 e
         * 16:9; L2 + R2 + Círculo troca a suavização. Ficam salvos. */
        static uint16_t before;
        uint16_t now = pad_bits | live_bits, pressed = now & ~before;
        before = now;
        if ((now & (PAD_L2 | PAD_R2)) == (PAD_L2 | PAD_R2) && (now & PAD_SELECT) == 0) {
            int changed = 0;
            if (pressed & PAD_CROSS) GsGpu_SetWide(!GsGpu_Wide()), changed = 1;
            if (pressed & PAD_CIRCLE) GsGpu_SetVisual((GsGpu_Visual() + 1) % 3), changed = 1;
            if (changed) {
                printf("ps2: suavizacao %d, 16:9 %d\n", GsGpu_Visual(), GsGpu_Wide());
                save_visual();
            }
        }
    }
    if ((getenv("PS2_DUMP_FRAME") && frame == (unsigned)atoi(getenv("PS2_DUMP_FRAME"))) || live_bits == 0xFFFF) {
        live_bits = 0;
        /* In strips of 32 rows (64 KB): with the translation's text on the
         * heap there is no room for the whole megabyte at once. */
        extern void GsGpu_ReadVramRows(int y, int rows, uint16_t *out);
        extern const uint16_t *SoftGpu_Vram(void);
        uint16_t *dump = memalign(64, 1024 * 32 * 2);
        FILE *file;
        char path[320];
        int y;
        if (!dump) {
            printf("ps2: VRAM dump: no memory\n");
            Ps2_LogSave();
            return;
        }
        if (live_input) snprintf(path, sizeof(path), "host:gsvram.bin");
        else snprintf(path, sizeof(path), "%s/YFM-GSVRAM.bin", program_dir);
        if ((file = fopen(path, "wb"))) {
            for (y = 0; y < 512; y += 32) {
                GsGpu_ReadVramRows(y, 32, dump);
                fwrite(dump, 2, 1024 * 32, file);
            }
            fclose(file);
        } else printf("ps2: VRAM dump: cannot write %s\n", path);
        if (live_input) snprintf(path, sizeof(path), "host:cpuvram.bin");
        else snprintf(path, sizeof(path), "%s/YFM-CPUVRAM.bin", program_dir);
        if ((file = fopen(path, "wb"))) {
            fwrite(SoftGpu_Vram(), 2, 1024 * 512, file);
            fclose(file);
        }
        free(dump);
        printf("ps2: dumped VRAM at frame %u\n", frame);
        Ps2_LogSave();
        {
            extern int GsGpu_Trace;
            if (live_input) GsGpu_Trace = 400;
        }
    }
    if (frame % 120 == 0) {
        unsigned tiles, count = GsGpu_Stats(&tiles);
        printf("ps2: gs: %u primitives, %u texture tiles decoded in 120 frames\n", count, tiles);
    }
}

/* ---- startup ---- */


static int exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

static const char *find_disc(void)
{
    static const char *const names[] = {"YFM.bin", "YFM.BIN", "fm.bin", "Yu-Gi-Oh! Forbidden Memories (USA).bin",
                                        "game/YFM.bin", "game/Yu-Gi-Oh! Forbidden Memories (USA).bin", NULL};
    static char path[512];
    const char *const *name;
    if (from_disc) {
        /* Na ISO o jogo é YFM.BIN na raiz do disco, pelo cdrom0: da BIOS. */
        static const char *const on_disc[] = {"cdrom0:\\YFM.BIN;1", NULL};
        for (name = on_disc; *name; name++)
            if (exists(*name)) return *name;
        return NULL;
    }
    for (name = names; *name; name++) {
        snprintf(path, sizeof(path), "%s/%s", program_dir, *name);
        if (exists(path)) return path;
    }
    for (name = names; *name; name++) {
        snprintf(path, sizeof(path), "host:%s", *name);
        if (exists(path)) return path;
    }
    return NULL;
}

int main(int argc, char **argv)
{
    char *slash;
    const char *disc;
    char user[300];
    static char *pc_argv[2] = {"memories-ps2", NULL};
    printf("ps2: Yu-Gi-Oh! Forbidden Memories PS2 port starting (%s)\n", argc > 0 ? argv[0] : "?");
    from_disc = argc > 0 && argv[0] && !strncmp(argv[0], "cdrom", 5);
    disc_mode = from_disc;
    iop_start();
    /* Sem a tela de texto de debug: as etapas vao so para o YFM-LOG.txt; a
     * tela mostra a abertura (intro_ps2.c) e, se algo falhar, Ps2_Fatal. */
    boot_screen = 0;
    Ps2_Stage("IOP: usb %d, controle %d, audio %d", usb_ok, pad_ok, audio_ok);
    {
        extern void Ps2_InstallFaultHandlers(void);
        Ps2_InstallFaultHandlers();
    }
    snprintf(program_dir, sizeof(program_dir), "%s", argc > 0 && argv[0] ? argv[0] : "host:");
    slash = strrchr(program_dir, '/');
    if (!slash) slash = strrchr(program_dir, '\\');
    if (slash) *slash = '\0';
    else if ((slash = strchr(program_dir, ':'))) slash[1] = '\0';
    if (!strncmp(program_dir, "mass:", 5) || !strncmp(program_dir, "host", 4)) {
        /* fine as it is */
    } else if (from_disc) {
        /* Os arquivos do port (saves, YFM-VISUAL.txt) no cartão de memória. */
        snprintf(program_dir, sizeof(program_dir), "mc%d:/YFMPS2", mc_ok ? cartao_gravavel() : 0);
    } else if (!program_dir[0]) {
        snprintf(program_dir, sizeof(program_dir), "mass:/YFM");
    }
    Ps2_Stage("pasta: %s", program_dir);
    /* The USB stick needs a moment to appear after its driver loads. */
    disc = find_disc();
    if (!disc && (usb_ok || disc_mode)) {
        int tries;
        for (tries = 0; tries < 20 && !(disc = find_disc()); tries++) {
            struct timespec wait = {0, 250000000};
            nanosleep(&wait, NULL);
        }
    }
    if (!disc) {
        char text[700];
        if (from_disc)
            snprintf(text, sizeof(text), "YFM.BIN was not found on this disc.\n\n  Make the ISO again with the port's\n"
                                         "  creator page (criar-port.html).");
        else
            snprintf(text, sizeof(text), "The disc image was not found.\n\n  Put YFM.bin (your Forbidden Memories USA .bin)\n"
                                         "  next to the ELF:\n  %s/YFM.bin", program_dir);
        Ps2_Fatal(text);
    }
    if (strncmp(program_dir, "host", 4) && !from_disc) { /* no cartão de memória, não: grava demais */
        snprintf(log_path, sizeof(log_path), "%s/YFM-LOG.txt", program_dir);
        if (freopen(log_path, "w", stdout)) {
            setvbuf(stdout, NULL, _IOLBF, 0);
            stderr = stdout;
        } else {
            log_path[0] = 0;
        }
    }
    Ps2_Stage("disco: %s", disc);
    if (from_disc && !mc_ok) printf("ps2: sem cartao de memoria: os saves nao vao ficar\n");
    if (from_disc) {
        /* O cdrom0: não responde fstat: o tamanho do YFM.BIN pelo stat do
         * caminho, ou pelo YFM.SIZ que o criador grava na ISO. */
        unsigned long long size = 0;
        struct stat st;
        FILE *file;
        if (!stat(disc, &st) && st.st_size > 0) size = (unsigned long long)st.st_size;
        if (!size && (file = fopen("cdrom0:\\YFM.SIZ;1", "rb"))) {
            char text[32] = {0};
            if (fgets(text, sizeof(text), file)) size = strtoull(text, NULL, 10);
            fclose(file);
        }
        if (size) {
            char text[32];
            snprintf(text, sizeof(text), "%llu", size);
            setenv("MEMORIES_DISC_SIZE", text, 1);
        }
        printf("ps2: disco no CD, %llu bytes\n", size);
    }
    load_visual();
    tela_ajustes();

    {
        extern void Ps2_Intro(int pad_ok, int audio_ok, int fast);
        FILE *want = strncmp(program_dir, "host", 4) ? NULL : fopen("host:intro.txt", "rb");
        if (want) fclose(want);
        if (strncmp(program_dir, "host", 4) || want) {
            Ps2_Stage("abertura");
            Ps2_Intro(pad_ok, audio_ok, 0);
            Ps2_Stage("abertura terminou");
        }
    }

    {
        struct timespec a, b;
        volatile unsigned spin;
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (spin = 0; spin < 3000000; spin++) {}
        clock_gettime(CLOCK_MONOTONIC, &b);
        printf("ps2: clock %ld.%09ld -> %ld.%09ld\n", (long)a.tv_sec, a.tv_nsec, (long)b.tv_sec, b.tv_nsec);
    }
    setenv("MEMORIES_DISC", disc, 1);
    setenv("MEMORIES_PROGRAM_DIR", program_dir, 1);
    if (!strcmp(program_dir, "host:")) snprintf(program_dir, sizeof(program_dir), "host:.");
    snprintf(user, sizeof(user), "%s/YFM-SAVE", program_dir);
    {
        /* Emulator test of the memory card path (host: with mctest.txt). */
        FILE *want = strncmp(program_dir, "host", 4) ? NULL : fopen("host:mctest.txt", "rb");
        if (want) fclose(want), disc_mode = 2;
    }
    if (disc_mode) {
        /* The memory card in slot 1 (or 2): mc0:/YFMPS2. */
        int slot = cartao_gravavel();
        snprintf(user, sizeof(user), "mc%d:/YFMPS2", slot);
        {
            /* O icone e o titulo da pasta no navegador do PS2 (sem eles o
             * console a mostra como dados corrompidos). */
            extern unsigned char mc_icon_sys[], mc_icon_sys_end[], mc_icon_icn[], mc_icon_icn_end[];
            static const struct { const char *name; unsigned char *start, *end; } icons[] = {
                {"icon.sys", mc_icon_sys, mc_icon_sys_end}, {"yfm.icn", mc_icon_icn, mc_icon_icn_end}};
            int k;
            for (k = 0; k < 2; k++) {
                char path[64];
                FILE *f;
                snprintf(path, sizeof(path), "%s/%s", user, icons[k].name);
                if ((f = fopen(path, "rb"))) {
                    /* ja existe: so regrava se for de outra versao */
                    long have;
                    fseek(f, 0, SEEK_END);
                    have = ftell(f);
                    fclose(f);
                    if (have == (long)(icons[k].end - icons[k].start))
                        continue;
                }
                if ((f = fopen(path, "wb"))) {
                    fwrite(icons[k].start, 1, (size_t)(icons[k].end - icons[k].start), f);
                    fclose(f);
                }
            }
        }
        if (disc_mode == 1) setenv("MEMORIES_TRACE", "mods", 1); /* o idioma no console do emulador */
        Ps2_Stage("disco: saves em %s", user);
    }
    setenv("MEMORIES_USER_DIR", user, 1);
    setenv("MEMORIES_NO_MONITOR", "1", 1);
    if (!strncmp(program_dir, "host:", 5)) {
        extern void Ps2_StartSampler(void);
        FILE *script = fopen("host:input.txt", "rb");
        live_input = 1;
        {
            /* host:env.txt: linhas CHAVE=VALOR para o código de PC (por
             * exemplo MEMORIES_MODE_AT, que pula para um modo do jogo). */
            FILE *env = fopen("host:env.txt", "rb");
            char line[256];
            while (env && fgets(line, sizeof(line), env)) {
                char *eq = strchr(line, '='), *end = line + strcspn(line, "\r\n");
                *end = 0;
                if (eq && eq > line) {
                    *eq = 0;
                    setenv(line, eq + 1, 1);
                    printf("ps2: env %s=%s\n", line, eq + 1);
                }
            }
            if (env) fclose(env);
        }
        setenv("PS2_GUEST_DUMP", "host:dumps/g", 1);
        setenv("MEMORIES_TRACE", "frames,disc,stub,window,clock,mods,input,model", 1);
        if (script) {
            static char input[4096];
            size_t got = fread(input, 1, sizeof(input) - 1, script);
            fclose(script);
            while (got && (input[got - 1] == '\n' || input[got - 1] == '\r')) got--;
            input[got] = '\0';
            setenv("MEMORIES_INPUT", input, 1);
            printf("ps2: scripted input %s\n", input);
        }
        if ((script = fopen("host:selftest.txt", "rb"))) {
            fclose(script);
            setenv("PS2_GS_SELFTEST", "1", 1);
        }
        if ((script = fopen("host:dump.txt", "rb"))) {
            static char at[32];
            size_t got = fread(at, 1, sizeof(at) - 1, script);
            fclose(script);
            at[got] = 0;
            setenv("PS2_DUMP_FRAME", at, 1);
        }
        Ps2_StartSampler();
    }
    return Memories_PcMain(1, pc_argv);
}

/* ---- the port's menus over the picture (gs_gpu.c draws it) ---- */
#include "pc/platform/menu.h"
#include "pc/saves/save_menu.h"
#include "pc/saves/deck_menu.h"
#define OV 3 /* the menus are drawn at three times the screen and reduced */
static uint32_t overlay_argb[320 * OV * 240 * OV];
/* The port's menus on the PS2: drawn on the CPU into an opaque box and
 * written into the shown picture's copy as plain pixels (gs_gpu.c), with no
 * texture and no blending on the GS. The box is the menu's own rectangle;
 * the area it may take is the middle of the screen (Menu_SetOverlayArea). */
static uint16_t overlay_box[320 * 240] __attribute__((aligned(64)));
const uint16_t *Ps2_OverlayBox(int *changed, int *bx, int *by, int *bw, int *bh)
{
    static unsigned last_signature;
    static int was_shown, x0, y0, w0, h0;
    int shown = SaveMenu_Active() || DeckMenu_Active() || Menu_NoticeShown();
    unsigned signature;
    if (!shown) {
        was_shown = 0;
        return NULL;
    }
    signature = SaveMenu_Signature() * 2654435761u ^ DeckMenu_Signature() * 40503u ^ (unsigned)Menu_NoticeShown();
    *changed = !was_shown || signature != last_signature;
    if (*changed) {
        /* Drawn at twice the screen's size and halved: the menu's text at
         * its smallest is 16 pixels, too big for a 320 x 240 picture. The
         * window takes the middle, at most 200 x 130 of the screen. */
        MenuCanvas canvas = {overlay_argb + 75 * OV * 320 * OV, 320 * OV, 320 * OV, 90 * OV, 0};
        MenuCanvas whole_screen = {overlay_argb, 320 * OV, 320 * OV, 240 * OV, 0};
        int x = 0, y = 0, w = 0, h = 0, row, i, j, n = 320 * OV * 240 * OV;
        for (i = 0; i < n; i++) overlay_argb[i] = 0x141a4au;
        Menu_SetOverlayArea(60 * OV, 260 * OV, 0);
        /* Save and load: their own window (save_menu.c), on the whole screen. */
        SaveMenu_DrawPs2(&whole_screen, &x, &y, &w, &h);
        if (!w || !h) {
            DeckMenu_Draw(&canvas, &x, &y, &w, &h);
            y += 75 * OV;
        }
        if (Menu_NoticeShown()) {
            Menu_Draw(&whole_screen);
            x = 0, y = 0, w = 320 * OV, h = 240 * OV;
        }
        /* To screen pixels (each the mean of OV x OV); whole qwords per row
         * for the GS transfer: x and w by 8 pixels. */
        if (x < 0) w += x, x = 0;
        if (y < 0) h += y, y = 0;
        w = (x + w + OV - 1) / OV - x / OV, h = (y + h + OV - 1) / OV - y / OV, x /= OV, y /= OV;
        w += x & 7, x &= ~7, w = (w + 7) & ~7;
        if (x + w > 320) w = 320 - x;
        if (y + h > 240) h = 240 - y;
        for (row = 0; row < h; row++)
            for (i = 0; i < w; i++) {
                unsigned r = 0, g = 0, bl = 0;
                for (j = 0; j < OV * OV; j++) {
                    uint32_t p = overlay_argb[(size_t)((y + row) * OV + j / OV) * 320 * OV + (size_t)(x + i) * OV + j % OV];
                    r += (p >> 16) & 255, g += (p >> 8) & 255, bl += p & 255;
                }
                r /= OV * OV, g /= OV * OV, bl /= OV * OV;
                overlay_box[row * w + i] = (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((bl >> 3) << 10));
            }
        x0 = x, y0 = y, w0 = w, h0 = h;
        last_signature = signature;
        was_shown = 1;
    }
    *bx = x0, *by = y0, *bw = w0, *bh = h0;
    return w0 > 0 && h0 > 0 ? overlay_box : NULL;
}
