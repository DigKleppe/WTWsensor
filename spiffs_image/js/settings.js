
var firstTime = true;
var firstTimeCal = true;

var body;
var infoTbl;
var calTbl;
var nameTbl;
var tblBody;
var INFOTABLENAME = "infoTable";
var CALTABLENAME = "calTable";
var NAMETABLENAME = "nameTable";



function makeNameTable(descriptorData) {
	var colls;
	nameTbl = document.getElementById(NAMETABLENAME);// ocument.createElement("table");
	var x = nameTbl.rows.length;
	for (var r = 0; r < x; r++) {
		nameTbl.deleteRow(-1);
	}
	tblBody = document.createElement("tbody");

	var rows = descriptorData.split("\n");

	for (var i = 0; i < rows.length - 1; i++) {
		var row = document.createElement("tr");
		if (i == 0) {
			colls = rows[i].split(",");
			for (var j = 0; j < colls.length; j++) {
				var cell = document.createElement("th");
				var cellText = document.createTextNode(colls[j]);
				cell.appendChild(cellText);
				row.appendChild(cell);
			}
		}
		else {
			var cell = document.createElement("td");
			var cellText = document.createTextNode(rows[i]);
			cell.appendChild(cellText);
			row.appendChild(cell);

			cell = document.createElement("td");
			var input = document.createElement("input");
			input.setAttribute("type", "text");
			cell.appendChild(input);
			row.appendChild(cell);

			cell = document.createElement("td");
			cell.setAttribute("nameItem", i);
			var button = document.createElement("button");  // name fixed
			button.innerHTML = "Stel in";
			button.className = "button-3";
			cell.appendChild(button);
			row.appendChild(cell);

			cell = document.createElement("td");
			cell.setAttribute("nameItem", i);
			cell.appendChild(button);
			row.appendChild(cell);
		}
		tblBody.appendChild(row);
	}
	nameTbl.appendChild(tblBody);

	const cells = document.querySelectorAll("td[nameItem]");
	cells.forEach(cell => {
		cell.addEventListener('click', function () { setNameFunction(cell.closest('tr').rowIndex, cell.cellIndex) });
	});
}

function makeInfoTable(descriptorData) {

	infoTbl = document.getElementById(INFOTABLENAME);// ocument.createElement("table");
	var x = infoTbl.rows.length
	for (var r = 0; r < x; r++) {
		infoTbl.deleteRow(-1);
	}
	tblBody = document.createElement("tbody");

	var rows = descriptorData.split("\n");

	for (var i = 0; i < rows.length - 1; i++) {
		var row = document.createElement("tr");
		var colls = rows[i].split(",");

		for (var j = 0; j < colls.length; j++) {
			if (i == 0)
				var cell = document.createElement("th");
			else
				var cell = document.createElement("td");

			var cellText = document.createTextNode(colls[j]);
			cell.appendChild(cellText);
			row.appendChild(cell);
		}
		tblBody.appendChild(row);
	}
	infoTbl.appendChild(tblBody);
}


function readInfo(str) {
	makeInfoTable(str);
}


function forgetWifi() {
	sendItem("forgetWifi");
}

function testInfo() {
	var str = "Meting,xActueel,xOffset,xx,\naap,2,3,4,\nnoot,5,6,7,\n,";
	readInfo(str);
	str = "Actueel,Nieuw,Stel in,Herstel,\nSensor 1\n";
	makeNameTable(str);
}


function initSettings() {
	if (SIMULATE) {
		testInfo();
	}
	setInterval(function () { settingsTimer() }, 1000);
	var name = getItem("getSensorName");
	document.title = "Info " + name;
}


var xcntr = 1;

function getInfo() {
	if (SIMULATE) {
		infoTbl.rows[1].cells[1].innerHTML = xcntr++;
		return;
	}
	var arr;
	var str;
	str = getItem("getInfoValues");
	if (firstTime) {
		makeInfoTable(str);
		firstTime = false;
	}
	else {
		var rows = str.split("\n");
		for (var i = 1; i < rows.length - 1; i++) {
			var colls = rows[i].split(",");
			for (var j = 1; j < colls.length; j++) {
				infoTbl.rows[i].cells[j].innerHTML = colls[j];
			}
		}
	}
}

function settingsTimer() {
	if (document.visibilityState == "hidden")
		return;
	getInfo();
}

function startStop() {
	calRun = !calRun;
	var button = document.getElementById("StartStopButton");
	if (calRun) {
		button.innerHTML = "** Stop  **";
		button.style.backgroundColor = "Red";
		sendItem("setCurrent=" + testCurrent);
	}
	else {
		button.innerHTML = "** Start **";
		button.style.backgroundColor = "Blue";
		sendItem("stopCal=1");
	}
}



