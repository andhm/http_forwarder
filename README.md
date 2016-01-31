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
-- http_forwarder.request_list_name //default "request_list"
-- http_forwarder.timeout // default 5000(ms)

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

