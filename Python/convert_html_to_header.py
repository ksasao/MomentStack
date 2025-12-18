# convert_html_to_header.py

def convert_html_to_header(input_file: str, output_file: str):
    with open(input_file, "r", encoding="utf-8") as f:
        lines = f.readlines()

    with open(output_file, "w", encoding="utf-8") as f:
        f.write("#ifndef index_html\n")
        f.write("#define index_html\n")
        f.write("String html=\"\\n\\\n")

        for line in lines:
            # 改行を削除
            stripped = line.rstrip("\n")
            # ダブルクォートをエスケープ
            escaped = stripped.replace('"', '\\"')
            # 出力
            f.write(escaped + "\\n\\\n")

        f.write("\";\n")
        f.write("#endif\n")

if __name__ == "__main__":
    # 例: index.html を index.html.h に変換
    convert_html_to_header("index.html", "index.html.h")
