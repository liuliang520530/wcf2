package router

import (
	"go_wechatFerry/app"

	"github.com/gin-gonic/gin"
)

type RouterGroup struct{}

var RouterGroupApp = new(RouterGroup) //把当前结构体都收藏起来，便于在main中调用初始化总路由

// InitRouter 初始化总路由
func (r *RouterGroup) Init(Router *gin.Engine) {
	apiRouter := Router.Group("/api")
	{
		messageRouter := apiRouter.Group("message")
		{
			// 基础消息配置
			messageRouter.POST("/callback/set", app.SetMessageCallbackUrl) // 设置消息回调地址
			messageRouter.GET("/types", app.GetMsgTypes)                   // 获取消息类型列表

			// 用户信息相关
			messageRouter.GET("/self/wxid", app.GetSelfWXID)       // 获取登录的wx_id
			messageRouter.GET("/self/info", app.GetUserInfo)       // 获取自己的信息
			messageRouter.GET("/contacts", app.GetContacts)        // 获取通讯录
			messageRouter.POST("/friend/accept", app.AcceptFriend) // 接受好友请求

			// 群组相关
			messageRouter.GET("/room/members/all", app.GetRoomMembersAll)     // 获取全部群的群成员
			messageRouter.POST("/room/member", app.GetRoomMember)             // 获取单个群成员列表
			messageRouter.POST("/room/member/add", app.AddChatroomMembers)    // 添加群成员
			messageRouter.POST("/room/member/invite", app.InvChatRoomMembers) // 邀请群成员
			messageRouter.POST("/room/member/delete", app.DelChatRoomMembers) // 删除群成员

			// 消息发送
			messageRouter.POST("/send/text", app.SendTxt)        // 发送文本消息
			messageRouter.POST("/send/image", app.SendIMG)       // 发送图片
			messageRouter.POST("/send/file", app.SendFile)       // 发送文件
			messageRouter.POST("/send/card", app.SendRichText)   // 发送卡片消息
			messageRouter.POST("/send/pat", app.SendPat)         // 发送拍一拍消息
			messageRouter.POST("/send/emotion", app.SendEmotion) // 发送emoji消息
			messageRouter.POST("/forward", app.ForwardMsg)       // 转发消息

			// 数据库操作
			messageRouter.GET("/db/names", app.GetDBNames)    // 获取数据库名
			messageRouter.POST("/db/tables", app.GetDBTables) // 获取表
			messageRouter.POST("/db/query", app.ExecDBQuery)  // 执行sql

			// 其他功能
			messageRouter.POST("/pyq/refresh", app.RefreshPyq)         // 刷新朋友圈
			messageRouter.POST("/attach/download", app.DownloadAttach) // 下载附件
		}
	}
}
