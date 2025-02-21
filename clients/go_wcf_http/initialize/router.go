package initialize

import (
	"go_wechatFerry/router"

	"github.com/gin-gonic/gin"
)

func InitRouter() *gin.Engine {
	r := gin.New()
	gin.SetMode(gin.ReleaseMode)

	// API路由分组
	apiGroup := r.Group("/api")

	// 注册各模块路由
	router.InitMessageRouter(apiGroup) // 消息相关路由
	router.InitUserRouter(apiGroup)    // 用户相关路由
	router.InitRoomRouter(apiGroup)    // 群组相关路由
	router.InitFileRouter(apiGroup)    // 文件相关路由
	router.InitDBRouter(apiGroup)      // 数据库相关路由

	return r
}
