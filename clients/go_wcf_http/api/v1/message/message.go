package message

import (
	"go_wechatFerry/service"

	"github.com/gin-gonic/gin"
)

// SendText 发送文本消息
func SendText(c *gin.Context) {
	var req service.SendTextRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(400, gin.H{"error": err.Error()})
		return
	}

	resp, err := service.SendText(req)
	if err != nil {
		c.JSON(500, gin.H{"error": err.Error()})
		return
	}

	c.JSON(200, resp)
}

// 其他消息处理API...
