package initialize

import (
	"go_wechatFerry/router"

	"github.com/gin-gonic/gin"
)

func InitRouter() *gin.Engine {
	r := gin.New()
	gin.SetMode(gin.ReleaseMode)

	// API路由分组
	PublicGroup := r.Group("")
	{
		// 健康检查
		PublicGroup.GET("/health", func(c *gin.Context) {
			c.JSON(200, "ok")
		})
	}

	// 注册路由组
	router.RouterGroupApp.Init(r)

	return r
}
