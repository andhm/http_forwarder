# http_forwarder
php for http forwarder

## Requirement
- PHP 5.2+
- Curl

## Install
```
$/path/to/phpize
$./configure --with-php-config=/path/to/php-config/
$make && make install
```
## Configure
- http_forwarder.request_list_name //default "request_list"
- http_forwarder.timeout // default 5000(ms)

## Sample (client)
POST DATA
```
request_list[t1][url]:http://www.baidu.com
request_list[t1][method]:get
request_list[t1][body]:
request_list[1][url]:http://www.qq.com
request_list[1][method]:get
request_list[1][body]:
```

## Sample (server)
```

// optional ini set
ini_set('http_forwarder.timeout', 3000);
ini_set('http_forwarder.request_list_name', "request_list");

$ret = http_forward();
if ($ret === false) {
	exit("http forwarder execute failed. errno=".hf_get_last_errno().", errmsg=".hf_get_last_errmsg());
}

var_dump($ret);
```
RESULT
```
array (size=2)
  't1' => 
		array (size=2)
			'code' => int 200
			'response' => string '<!DOCTYPE html><!--STATUS OK--><html><head><meta http-equiv="content-type" content="text/html;charset=utf-8"><meta http-equiv="X-UA-Compatible" content="IE=Edge"><meta content="always" name="referrer"><meta name="theme-color" content="#2932e1"><link rel="shortcut icon" href="/favicon.ico" type="image/x-icon" /><link rel="search" type="application/opensearchdescription+xml" href="/content-search.xml" title="鐧惧害鎼滅储" /><link rel="icon" sizes="any" mask href="//www.baidu.com/img/baidu.svg"><link rel="dns-'... (length=98290)
	1 => 
		array (size=2)
			'code' => int 200
			'response' => string '<!DOCTYPE html>
					<html lang="zh-CN">
					<head>
					<meta content="text/html; charset=gb2312" http-equiv="Content-Type">
					<meta http-equiv="X-UA-Compatible" content="IE=edge">
					<title>腾讯首页</title>
					<script type="text/javascript">
						if(window.location.toString().indexOf('pref=padindex') != -1){}else{
							if(/AppleWebKit.*Mobile/i.test(navigator.userAgent) || (/MIDP|SymbianOS|NOKIA|SAMSUNG|LG|NEC|TCL|Alcatel|BIRD|DBTEL|Dopod|PHILIPS|HAIER|LENOVO|MOT-|Nokia|SonyEricsson|SIE-|Amoi|ZTE/.test(navigator.userAgent))){  
							'... (length=619504)
```





#### 企业未读数方案
####现状：
**1. 聊天关系表记录 11w+**
**2. 聊天消息表记录 29w+**
**3. 企业建立的聊天关系**

>a. top 30 
```mysql
+-----+------------+
| num | account_id |
+-----+------------+
| 738 |      63719 |
| 636 |      69010 |
| 563 |      69600 |
| 538 |      55096 |
| 502 |      60916 |
| 458 |      20189 |
| 364 |      64184 |
| 314 |      68347 |
| 280 |      69295 |
| 277 |      61162 |
| 250 |      67400 |
| 248 |      51547 |
| 244 |      45166 |
| 239 |      70764 |
| 232 |      63764 |
| 229 |      18359 |
| 227 |      71323 |
| 211 |      52535 |
| 194 |      52011 |
| 177 |      16739 |
| 165 |      49055 |
| 162 |      12547 |
| 161 |      21103 |
| 158 |      20027 |
| 158 |      70864 |
| 158 |      55733 |
| 155 |      51356 |
| 154 |      71198 |
| 154 |      70152 |
| 154 |      65334 |
+-----+------------+
```

> b. 平均
```mysql
+---------------+
| avg(test.num) |
+---------------+
|        9.8914 |
+---------------+
```

**4. 当前企业获取消息数的方式**

    SELECT COUNT(t_chat_message.MSG_ID) FROM t_chat_message LEFT JOIN t_chat_friend ON (t_chat_message.F_ID=t_chat_friend.F_ID AND t_chat_message.MSG_ID>t_chat_friend.CORP_UNREAD_ID AND t_chat_friend.CORP_UNREAD_ID>0) WHERE t_chat_message.ACCOUNT_ID=63719 AND t_chat_message.SENDER=1 AND t_chat_friend.DEL_STATUS IN (0,1)
	
**5. 当前企业获取好友列表（包括每个好友的未读标记）**

    SELECT t_chat_friend.MSG_LAST_TIME as lastTime, t_user_member.B_REAL_NAME as realName, t_user_member.USER_ID as userID, t_user_member.R_PHOTO as photo, t_user_member.G_JOB_NAME as userJobName, t_chat_friend.JOB_ALIAS_NAME as jobName, t_chat_friend.JOB_ID as jobID, t_chat_friend.CORP_UNREAD_ID, t_chat_friend.USER_UNREAD_ID, t_chat_friend.F_ID, max(t_chat_message.MSG_ID) as msgID FROM t_chat_friend LEFT JOIN t_user_member ON (t_chat_friend.USER_ID=t_user_member.USER_ID) LEFT JOIN t_chat_message ON (t_chat_friend.F_ID=t_chat_message.F_ID and t_chat_message.SENDER = 1) WHERE t_chat_friend.ACCOUNT_ID=63719 AND t_chat_friend.DEL_STATUS IN (0,1) GROUP BY t_chat_friend.F_ID ORDER BY t_chat_friend.MSG_LAST_TIME DESC LIMIT 20

>提示：目前好友未读只是标记，没有直接数量展示，做法是直接通过 t_chat_friend 表的 CORP_UNREAD_ID>0 判断是否有未读


####方案：
**1. 维持现有查询方式不变，企业登录查询一次，存入session，5分钟更新一次该消息计数（短期缓存）**
**2. 聊天关系表 t_chat_friend 增加"企业未读数"字段(CORP_UNREAD_CNT)，发送消息该字段值+1，已读该字段值清空为0**

> a. 获取企业未读数
    
	SELECT SUM(t_chat_friend.CORP_UNREAD_CNT) AS TOTAL_UNREAD_CNT FROM t_chat_friend WHERE t_chat_friend.ACCOUNT_ID=63719 AND t_chat_friend.DEL_STATUS IN (0,1)
	
> b. 获取聊天列表里用户的未读数
    
	SELECT t_chat_friend.CORP_UNREAD_CNT AS UNREAD_CNT FROM t_chat_friend WHERE t_chat_friend.ACCOUNT_ID=63719
	
> 提示：方案一 “获取聊天列表里用户的未读数” 获取方式如下，且 t_chat_friend 表里 corp_unread_id 不能再被重置为0 （即已读请零操作）

	SELECT COUNT(t_chat_message.MSG_ID) AS UNREAD_CNT, t_chat_friend.USER_ID FROM t_chat_friend LEFT JOIN t_chat_message ON (t_chat_friend.F_ID=t_chat_message.F_ID AND t_chat_message.SENDER=1) WHERE t_chat_friend.ACCOUNT_ID=63719 AND t_chat_message.MSG_ID>t_chat_friend.CORP_UNREAD_ID AND t_chat_friend.DEL_STATUS IN (0, 1) GROUP BY t_chat_friend.F_ID LIMIT 20;
