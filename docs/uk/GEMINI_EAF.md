# Gemini EAF — v0.2.10

Канонічний документ: `../GEMINI_EAF.md`.

Основний шлях — native `oal.gemini` через перевірений serial/MyFocuserPro2-compatible transport. INDI/Alpaca лишаються fallback. Capabilities визначаються реальним handshake/firmware, неперевірені команди не оголошуються підтриманими. Особлива увага — достовірності absolute position після power cycle.
