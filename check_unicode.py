#!/usr/bin/env python
# -*- coding: utf-8 -*-

# 检查"──"符号的Unicode码点
symbol = "──"
print(f"符号: {symbol}")
print(f"长度: {len(symbol)}")
for i, char in enumerate(symbol):
    print(f"字符 {i}: '{char}' = U+{ord(char):04X}")
    
print("\n相关符号:")
related = ["─", "━", "―", "—", "–", "-", "－"]
for char in related:
    print(f"'{char}' = U+{ord(char):04X}")
