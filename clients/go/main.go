package main

import (
	// "github.com/danbai225/WeChatFerry-go/wcf"
	"fyne.io/fyne/v2/app"
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
	green := color.NRGBA{R: 0, G: 180, B: 0, A: 255}
	w.SetContent(widget.NewLabel("Hello, Fyne!"))
	w.ShowAndRun()
}
