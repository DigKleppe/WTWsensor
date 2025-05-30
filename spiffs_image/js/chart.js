var data;
var chartRdy = false;
var tick = 0;
var dontDraw = false;
var halt = false;
var chartHeigth = 500;
var simValue1 = 0;
var simValue2 = 0;
var table;
var presc = 1;
var simMssgCnts = 0;
var lastTimeStamp = 0;
var REQINTERVAL = (5 * 60); // sec
var firstRequest = true;
var plotTimer = 6; // every 60 seconds plot averaged value
var rows = 0;
var chargeInfoTbl;

var SECONDSPERTICK = (5 * 60);// log interval 
var LOGDAYS = 1;
var MAXPOINTS = (LOGDAYS * 24 * 60 * 60 / SECONDSPERTICK)

var dayNames = ['zo', 'ma', 'di', 'wo', 'do', 'vr', 'za'];
var displayNames = ["", "1", "2", "3", "4"];
var cbIDs = ["", "T1cb", "T2cb", "T3cb", "T4cb"];

var chartSeries = [-1, -1, -1, -1, -1];

var NRItems = displayNames.length;


var chartOptions = {
	title: '',
	curveType: 'function',
	legend: { position: 'bottom' },

	heigth: 200,
	crosshair: { trigger: 'both' },	// Display crosshairs on focus and selection.
	explorer: {
		actions: ['dragToZoom', 'rightClickToReset'],
		//actions: ['dragToPan', 'rightClickToReset'],
		axis: 'horizontal',
		keepInBounds: true,
		maxZoomIn: 100.0
	},
	chartArea: { 'width': '90%', 'height': '60%' },

	vAxes: {
		0: { logScale: false },
	},
};

function clear() {
	data.removeRows(0, data.getNumberOfRows());
	chart.draw(data, chartOptions);
	tick = 0;
}

//var formatter_time= new google.visualization.DateFormat({formatType: 'long'});
// channel 1 .. 5

function plot(channel, value) {
	if (chartRdy) {
		if (channel == 1) {
			data.addRow();
			if (data.getNumberOfRows() > MAXPOINTS == true)
				data.removeRows(0, data.getNumberOfRows() - MAXPOINTS);
		}
		if (value != '--') {
			if (value > -50.0) {
				value = parseFloat(value); // from string to float
				data.setValue(data.getNumberOfRows() - 1, channel, value);
			}
		}
	}
}

function loadCBs() {
	var cbstate;

	console.log("Reading CBs");

	// Get the current state from localstorage
	// State is stored as a JSON string
	cbstate = JSON.parse(localStorage['CBState'] || '{}');

	// Loop through state array and restore checked 
	// state for matching elements
	for (var i in cbstate) {
		var el = document.querySelector('input[name="' + i + '"]');
		if (el) el.checked = true;
	}

	// Get all checkboxes that you want to monitor state for
	var cb = document.getElementsByClassName('save-cb-state');

	// Loop through results and ...
	for (var i = 0; i < cb.length; i++) {

		//bind click event handler
		cb[i].addEventListener('click', function(evt) {
			// If checkboxe is checked then save to state
			if (this.checked) {
				cbstate[this.name] = true;
			}

			// Else remove from state
			else if (cbstate[this.name]) {
				delete cbstate[this.name];
			}

			// Persist state
			localStorage.CBState = JSON.stringify(cbstate);
		});
	}
	console.log("CBs read");
	initTimer();
};


function initChart() {
	window.addEventListener('load', loadCBs());

}

function initTimer() {
	var activeSeries = 1;
	chart = new google.visualization.LineChart(document.getElementById('chart'));
	data = new google.visualization.DataTable();
	data.addColumn('string', 'Time');

	for (var m = 1; m < NRItems; m++) { // time not used for now 
		var cb = document.getElementById(cbIDs[m]);
		if (cb) {
			if (cb.checked) {
				data.addColumn('number', displayNames[m]);
				chartSeries[m] = activeSeries;
				activeSeries++;
			}
		}
	}
	if (activeSeries == 1) { // then none selected, select first
		var cb = document.getElementById(cbIDs[1]);
		data.addColumn('number', displayNames[1]);
		chartSeries[1] = 1;
		cb.checked = true;
	}

	chartRdy = true;
	makeChargeInfoTable();
	if (SIMULATE) {
		simplot();
	}
	else {
		setInterval(function() { timer() }, 1000);
	}
}


function updateLastDayTimeLabel(data) {
	var ms = Date.now();
	var date = new Date(ms);
	var labelText = date.getHours() + ':' + date.getMinutes();
	data.setValue(data.getNumberOfRows() - 1, 0, labelText);
}

function updateAllDayTimeLabels(data) {
	var rows = data.getNumberOfRows();
	var secondsAgo = rows * SECONDSPERTICK;
	var ms = Date.now();
	ms -= (secondsAgo * 1000);
	for (var n = 0; n < rows; n++) {
		var date = new Date(ms);
		var labelText = dayNames[date.getDay()] + ';' + date.getHours() + ':' + date.getMinutes();
		data.setValue(n, 0, labelText);
		ms += 1000 * SECONDSPERTICK;
	}
}

function simplot() {
	var nrLines;

	if (firstRequest) {
		nrLines = 50;
		firstRequest = false;
	}
	else
		nrLines = 1;
	var str = '';
	for (var m = 0; m < nrLines; m++) {
		tick++;
		str = str + tick + ',';
		for (var n = 0; n < NRItems; n++) {  // voltages
			simValue1 += 0.1;
			simValue2 = 1 + Math.sin(simValue1) * 0.4;
			str = str + simValue2;
			if (n < (NRItems - 1))
				str = str + ',';
			else
				str = str + '\n';
		}
	}
	plotArray(str);
}

function plotArray(str) {
	var arr;
	var arr2 = str.split("\n");
	var nrPoints = arr2.length - 1;
	if (nrPoints > 0) {
		for (var p = 0; p < nrPoints; p++) {
			arr = arr2[p].split(",");
			if (arr.length >= NRItems) {
				for (var m = 1; m < NRItems; m++) { // time not used for now
					if (chartSeries[m] != -1)
						plot(chartSeries[m], arr[m]);
				}
			}
		}
		if (nrPoints == 1) { // then single point added 
			updateLastDayTimeLabel(data);
		}
		else {
			updateAllDayTimeLabels(data);
		}
		chart.draw(data, chartOptions);
	}
}

function timer() {
	var arr;
	var arr2;
	var str;
	var chargeInfoTbl;
	if (SIMULATE) {
		simplot();
	}
	else {
		if (firstRequest) {
			arr = getItem("getLogMeasValues");
			plotArray(arr);
			firstRequest = false;

			arr = getItem("getFunction");  // set function RB

			var rb = document.getElementsByClassName('rw-rb-state');
			for (var i = 0; i < rb.length; i++) {
				if (arr == rb[i].value)
					rb[i].checked = true;
			}
		}
		chargeInfoTbl = document.getElementById("chargeInfoTable");
		str = getItem("getChargeValues");
		arr = str.split("\n");
		for (var colls = 0; colls < arr.length-1; colls++) {  
			arr2 = arr[colls].split(",");
			for (var m = 0; m < arr2.length; m++)
				chargeInfoTbl.rows[m + 1].cells[colls + 1].innerHTML = arr2[m];
		}
		presc--;

		if (presc == 0) {
			presc = REQINTERVAL;
			str = getItem("getRTMeasValues");
			arr = str.split(",");
			// print RT values xx
			if (arr.length >= NRItems) {
				if (arr[0] > 0) {
					if (arr[0] != lastTimeStamp) {
						lastTimeStamp = arr[0];
						for (var m = 1; m < NRItems; m++) { // time not used for now 
							if (chartSeries[m] != -1)
								plot(chartSeries[m], arr[m]);
						}
						updateLastDayTimeLabel(data);
						chart.draw(data, chartOptions);
					}
				}
			}

		}
	}
}

function functionRbClick(val) {
	sendItem("setFunction=" + val);
}

function startStop() {
	halt = !halt;
	if (halt)
		document.getElementById('startStopButton').innerHTML = 'start';
	else
		document.getElementById('startStopButton').innerHTML = 'stop';
}

function clearLog() {
	sendItem("clearLog=1");
	clear();
}

function clearChart() {
	clear();
}
function refreshChart() {
	data.removeRows(0, data.getNumberOfRows());
	arr = getItem("getLogMeasValues");
	plotArray(arr);
}


function makeChargeInfoTable() {
	var str;
	if (SIMULATE) {
		str = "Meting,xActueel,xOffset,xx\n rij1, rij2 , rij3\n";

	}
	else {
		str = getItem("getChargeTable");
	}
	chargeInfoTbl = makeTable("chargeInfoTable", str);
}

function makeTable(tableID, descriptorData) {

	var tableName = document.getElementById(tableID);
	var x = tableName.rows.length
	for (var r = 0; r < x; r++) {
		tableName.deleteRow(-1);
	}
	tblBody = document.createElement("tbody");
	var lines = descriptorData.split("\n");

	var colls = lines[0].split(",");
	var rows = lines[1].split(",");

	for (var i = 0; i < rows.length + 1; i++) {
		var row = document.createElement("tr");
		for (var j = 0; j < colls.length; j++) {
			if (i == 0) {
				var cell = document.createElement("th");
				var cellText = document.createTextNode(colls[j]);
				cell.appendChild(cellText);
			}
			else {
				if (j == 0) {
					var cell = document.createElement("th");
					var cellText = document.createTextNode(rows[i - 1]);
					cell.appendChild(cellText);
				}
				else {
					var cell = document.createElement("td");
				}
			}
			row.appendChild(cell);
		}
		tblBody.appendChild(row);
	}
	tableName.appendChild(tblBody);
	return tableName;
}
