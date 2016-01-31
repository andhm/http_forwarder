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
																		if(window.location.toString().indexOf('pref=padindex') != -1){
																		}else{
																							if(/AppleWebKit.*Mobile/i.test(navigator.userAgent) || (/MIDP|SymbianOS|NOKIA|SAMSUNG|LG|NEC|TCL|Alcatel|BIRD|DBTEL|Dopod|PHILIPS|HAIER|LENOVO|MOT-|Nokia|SonyEricsson|SIE-|Amoi|ZTE/.test(navigator.userAgent))){  
																											      '... (length=619504)
```
