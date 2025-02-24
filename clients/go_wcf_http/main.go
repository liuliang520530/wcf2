package main

import (
	"go_wechatFerry/app"
	"go_wechatFerry/initialize"
	"os"
	"os/signal"
)

func main() {
	// 注册信号处理
	signalChan := make(chan os.Signal, 1)
	signal.Notify(signalChan, os.Interrupt)

	// 初始化路由
	router := initialize.InitRouter()

	// 启动消息监听
	// go app.OnMsg()

	// 启动HTTP服务
	go router.Run(":8000")

	// 优雅退出
	<-signalChan
	app.WxClient.Close()
}
