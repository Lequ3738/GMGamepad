# GMGamepad

在 GameMaker 8.0 中，对手柄的支持很糟糕：<br>
<ul>
<li>对手柄的控制非常底层，基本把手柄视作一堆按钮与摇杆的集合。</li>
<li>插入了手柄且未启用时会导致游戏变慢（CPU 资源占用极大）。</li>
<li>如果使用了 gm8x_fix 补丁则会导致 GM8 中自带的手柄支持功能失效。</li>
</ul>
拥有以上特性，使得 GM8 中自带的手柄支持功能几乎无法使用。此扩展就是为了解决以上痛点，并为了更加方便地使用手柄功能而生。<br>
此扩展的 API 尽量贴合现代 GameMaker API（即前 GameMaker Studio 2 的 API），减少学习成本。

## 如何编译

使用 Visual Studio 2022 进行编译。<br>
注意要使用 x86 平台进行编译，因为 GameMaker 8.0 是 32 位程序。

并且要下载  [SDL3](https://github.com/libsdl-org/SDL/releases/tag/release-3.2.18)，将其中的 `SDL3.dll` 和 `SDL3.lib` 放置在工程文件夹下。

## 如何使用
插件的详细用法请参见插件文件夹下的 `GM_Gamepad.chm` 文档。

## 感谢
感谢以下项目提供的灵感和代码参考：
- [**jm82joy**](https://github.com/GM82Project/gm82joy)
