
import sys
from PIL import Image

if len(sys.argv) < 3:
    print("Uso: python3 converter.py <imagem.png> <NOME_DA_VARIAVEL>")
    sys.exit(1)

arquivo_entrada = sys.argv[1]
nome_var = sys.argv[2].upper()
arquivo_saida = nome_var.lower() + ".hpp"

img = Image.open(arquivo_entrada).convert("RGBA")
w, h = img.size

with open(arquivo_saida, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <cstdint>\n\n")
    f.write(f"const int {nome_var}_W = {w};\n")
    f.write(f"const int {nome_var}_H = {h};\n")
    f.write(f"const uint32_t {nome_var}_DATA[] = {{\n")

    for y in range(h):
        linha = "  "
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            if a < 128:
                cor = 0xFF000000 # Pixel transparente
            else:
                cor = (r << 16) | (g << 8) | b # Formato 0x00RRGGBB do seu motor
            linha += f"0x{cor:08X}, "
        f.write(linha + "\n")

    f.write("};\n")

print(f"Sucesso! Gerado '{arquivo_saida}' ({w}x{h} pixels).")
