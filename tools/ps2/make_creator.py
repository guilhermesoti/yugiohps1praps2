#!/usr/bin/env python3
"""Gera release/criar-port.html: a página do criador (creator.html) com o
núcleo (core.js) e o ELF do port embutidos (gzip + base64).

    python3 tools/ps2/make_creator.py [ELF] [saída]
"""
import base64, datetime, gzip, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def version():
    stamp = datetime.date.today().strftime("%d/%m/%Y")
    try:
        commit = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], capture_output=True, text=True,
                                check=True).stdout.strip()
        dirty = subprocess.run(["git", "-C", ROOT, "status", "--porcelain", "--untracked-files=no"],
                               capture_output=True, text=True).stdout.strip()
        return f"{stamp} ({commit}{'+' if dirty else ''})"
    except (OSError, subprocess.CalledProcessError):
        return stamp


def main():
    elf_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "release", "YFM_PS2.ELF")
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "release", "criar-port.html")
    with open(os.path.join(HERE, "creator", "creator.html"), encoding="utf-8") as f:
        page = f.read()
    with open(os.path.join(HERE, "creator", "core.js"), encoding="utf-8") as f:
        core = f.read()
    with open(elf_path, "rb") as f:
        elf = f.read()
    b64 = base64.b64encode(gzip.compress(elf, 9, mtime=0)).decode("ascii")
    # A tradução vai junto (languages/pt-br.txt): na ISO como PTBR.TXT, no
    # aplicativo em languages/. Sem o arquivo, o port sai só em inglês.
    ptbr_path = os.path.join(ROOT, "languages", "pt-br.txt")
    ptbr = ""
    if os.path.exists(ptbr_path):
        with open(ptbr_path, "rb") as f:
            ptbr = base64.b64encode(gzip.compress(f.read(), 9, mtime=0)).decode("ascii")
    for key, value in (("/*__CORE__*/", core), ("__ELF_B64__", b64), ("__PTBR_B64__", ptbr),
                       ("__VERSION__", version())):
        if key not in page:
            sys.exit(f"criar-port: {key} não está em creator.html")
        page = page.replace(key, value, 1)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(page)
    print(f"{out_path}: {len(page) / 1048576:.1f} MB (ELF de {len(elf) / 1048576:.1f} MB embutido)")


if __name__ == "__main__":
    main()
