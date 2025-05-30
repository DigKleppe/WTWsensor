//var SIMULATE = true;
var SIMULATE = false;
var data2 = 0;
var simValue1 = 0;
var simValue2 = 0;

var ACTCOLLUMN = 1;
var OFFSETCOLLUN = 2;
var TEMPINFOROW = 1;
var RHINFOROW = 2;
var CO2INFOROW = 3;

function sendItem(item) {
	console.log("sendItem: " + item);
	if (SIMULATE)
		return "OK";

	var str;
	var req = new XMLHttpRequest();
	req.open("POST", "/upload/cgi-bin/", false);
	req.send(item);
	if (req.readyState == 4) {
		if (req.status == 200) {
			str = req.responseText.toString();
			return str;
		}
	}
}

function getItem(item) {
	console.log("getItem " + item);
	if (SIMULATE)
		return "OK";

	var str;
	var req = new XMLHttpRequest();
	req.open("GET", "cgi-bin/" + item, false);
	req.send();
	if (req.readyState == 4) {
		if (req.status == 200) {
			str = req.responseText.toString();
			return str;
		}
	}
}



