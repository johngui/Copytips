# Copytips
Windows小软件，当复制文字时屏幕出现弹窗提醒

<img width="359" height="121" alt="Snipaste_2025-08-02_21-15-53" src="https://github.com/user-attachments/assets/da5f9fde-5c5e-4e72-8ea4-9990d486483e" />
<img width="318" height="121" alt="Snipaste_2025-08-02_21-15-31" src="https://github.com/user-attachments/assets/cc3a36cd-ac57-4fc9-8a07-b2eab5a59f2a" />
运行后会常驻系统托盘，一旦检测到剪贴板中有新的纯文本复制操作，就在鼠标附近弹出一个半透明的 Toast 提示“已复制”。
特点：
单文件即可运行，无额外依赖（仅使用标准库 + ctypes）。
最小化到托盘，右键可退出。
纯文本触发，避免图片/文件等非文本内容打扰。
用 C/C++ 直接调用 Win32 API，可编译成 单个 100 KB 以内 的 x64 可执行文件；
不轮询剪贴板，而是利用 Windows 提供的 剪贴板监听机制（AddClipboardFormatListener），CPU 占用≈0；
用纯 Win32 创建一个 WS_EX_TOOLWINDOW 风格的 2 秒自毁 Toast 窗口（无框架、半透明、置顶、不抢焦点）；
用 静态链接 CRT（/MT），运行时不依赖任何运行时库，拷到任何 Win11 机器即可双击运行。
