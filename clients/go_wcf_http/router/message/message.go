package message

import (
	v1 "go_wechatFerry/api/v1/message"

	"github.com/gin-gonic/gin"
)

func InitMessageRouter(Router *gin.RouterGroup) {
	messageRouter := Router.Group("message")
	{
		messageRouter.POST("/send/text", v1.SendText)   // 发送文本
		messageRouter.POST("/send/image", v1.SendImage) // 发送图片
		messageRouter.POST("/send/file", v1.SendFile)   // 发送文件
		// ... 其他消息相关路由
	}
}
