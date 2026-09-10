# Meizu Power Control

GTK 4/libadwaita 电源状态工具，面向运行 mainline Linux 的 Meizu 20 Infinity。
它通过标准 power_supply 和 Type-C sysfs 显示：

- 无线反向充电开关、发射状态、电压和电流
- 无线充电接收状态
- 电池电量、充放电状态、功率和温度
- USB 供电及 Type-C 角色、方向和 USB PD 状态

反向充电开关通过一个只允许写入 `0` 或 `1` 的小型 polkit helper 修改
`/sys/class/power_supply/qcom-battmgr-wls-tx/online`，主界面无需以 root 运行。

## 构建

Arch Linux / Arch Linux ARM：

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf gtk4 libadwaita
meson setup build --prefix=/usr
meson compile -C build
```

## 安装

```sh
sudo meson install -C build
```

运行 `meizu-power-control`。第一次切换无线反向充电时，polkit 会请求认证。
