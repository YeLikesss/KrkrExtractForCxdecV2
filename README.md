# KrkrExtractV2 (ForCxdecV2) 动态解包工具
&emsp;&emsp;适用于Wamsoft(Hxv4 2021.11+)加密解包

## 环境
&emsp;&emsp;系统: Windows 7 SP1 x64

&emsp;&emsp;IDE: Visual Sudio 2022

&emsp;&emsp;编译器: MSVC2022 x86

## 与老版本区别
&emsp;&emsp;1.重构代码, 稍微看着没那么屎山

&emsp;&emsp;2.功能拆分, 此项目仅支持解包, 无其他功能

&emsp;&emsp;3.修复bug

## 如何使用
&emsp;&emsp;1. `CxdecExtractorLoader.exe`, `CxdecExtractor.dll`, `CxdecExtractorUI.dll`保持同一目录

&emsp;&emsp;2. 保证你的游戏是Wamsoft KrkrZ Hxv4加密类型且加密认证已移除

&emsp;&emsp;3. 拖拽xxx.exe到`CxdecExtractorLoader.exe`启动, 弹出解包响应框, 拖拽xxx.xp3到响应框内解包

&emsp;&emsp;4. 程序不会申请管理员权限弹出UAC提权, 游戏与工具务必不要放在C盘

&emsp;&emsp;5. 提取后游戏资源位于`游戏目录\Extractor_Output\`文件夹, 包含xxx文件夹的封包资源与xxx.alst的文件表

&emsp;&emsp;6. 提取日志信息为`工具目录\Extractor.log`文件

&emsp;&emsp;7. 如出现错误标题的弹窗报错, 请检查上述步骤

## 常见问题
&emsp;&emsp;Q: 为什么没有资源文件名

&emsp;&emsp;A: 封包里面本来就没有文件名

&emsp;&emsp;Q: 解包响应框支持批量拖拽解包吗

&emsp;&emsp;A: 不支持, 仅支持单个封包逐个拖拽提取

&emsp;&emsp;Q: 解包响应框解包时候无响应

&emsp;&emsp;A: 没做多线程支持, 等它解完就好

&emsp;&emsp;Q: 兼容Win7以外的系统吗

&emsp;&emsp;A: 理论上兼容, 不过没有测试, 有问题我也不知道

## 同类工具推荐

 * KrkrExtractV2 (ForCxdecV2)(本工具) &emsp; 类型: 动态  &emsp; 解包: 一次性 &emsp;文件名: 无

 * [KrkrDump](https://github.com/crskycode/KrkrDump) &emsp; 类型: 动态 &emsp; 解包: 运行时 &emsp;文件名: 有

 * [GARBro](https://github.com/crskycode/GARbro) &emsp; 类型: 静态(需人工装填) &emsp; 解包: 一次性 &emsp;文件名: 无