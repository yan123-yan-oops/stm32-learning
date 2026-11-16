"""让链接阶段也使用硬浮点 ABI。

PlatformIO 把 build_flags 只传给编译器、不传给链接器，
而 framework 的目标文件是硬浮点编译的，链接时会报：
  "uses VFP register arguments, firmware.elf does not"
"""

Import("env")

env.Append(LINKFLAGS=["-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"])
