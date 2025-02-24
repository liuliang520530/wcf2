package main

import (
	// "github.com/danbai225/WeChatFerry-go/wcf"
	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"
)

func main() {
	// c, err := wcf.NewWCF("")
	// if err != nil {
	// 	panic(err)
	// }
	// println(c.IsLogin())
	a := app.New()
	w := a.NewWindow("bot")
	w.Resize(fyne.NewSize(1241, 733))
	w.SetTitle("AI微信机器人")
	// green := color.NRGBA{R: 0, G: 180, B: 0, A: 255}
	// 左侧控制单元
	controlPanel := container.NewVBox(
		widget.NewLabel("控制单元"),
		widget.NewButton("按钮1", func() {
			// 按钮1的点击事件
		}),
		widget.NewButton("按钮2", func() {
			// 按钮2的点击事件
		}),
	)

	// 设置按钮样式
	for _, obj := range controlPanel.Objects {
		if button, ok := obj.(*widget.Button); ok {
			button.Importance = widget.HighImportance
			button.Style = widget.PrimaryButton
		}
	}

	// 右侧消息内容
	messageContent := container.NewVBox(
		widget.NewLabel("消息内容"),
		widget.NewEntry(),
	)

	// 水平分割
	split := container.NewHSplit(controlPanel, messageContent)
	split.Offset = 0.3 // 设置分割位置，0.3表示左侧占30%

	w.SetContent(split)
	w.ShowAndRun()
}
