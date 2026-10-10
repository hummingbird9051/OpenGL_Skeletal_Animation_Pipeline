target_file = "PostProcessing.h"

# 1. 파일이 CP949로 되어 있으므로 cp949로 읽습니다.
with open(target_file, "r", encoding="cp949", errors="replace") as f:
    text = f.read()

# 2. Visual Studio가 인식할 수 있는 UTF-8(BOM)으로 덮어씁니다.
with open(target_file, "w", encoding="utf-8-sig") as f:
    f.write(text)

print("Model.h 복구 완료!")
