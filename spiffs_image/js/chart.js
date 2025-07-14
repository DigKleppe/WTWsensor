var CO2Data;
var tempAndRHdata;
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

var firstRequest = true;
var plotTimer = 6; // every 60 seconds plot averaged value
var rows = 0;


var MINUTESPERTICK = 1;// log interval 
var LOGDAYS = 7;
var MAXPOINTS = LOGDAYS * 24 * 60 / MINUTESPERTICK;

var SIMULATE = false;

var displayNames = ["", "CO2", "temperatuur", "RH"];
var unit = ["", " ppm", " °C", " %"];
var dayNames = ['zo', 'ma', 'di', 'wo', 'do', 'vr', 'za'];

var CO2Options = {
	title: '',
	curveType: 'function',
	legend: { position: 'top' },
	heigth: 200,
	crosshair: { trigger: 'both' },	// Display crosshairs on focus and selection.
	explorer: {
		actions: ['dragToZoom', 'rightClickToReset'],
		//actions: ['dragToPan', 'rightClickToReset'],
		axis: 'horizontal',
		keepInBounds: true,
		maxZoomIn: 100.0
	},
	chartArea: { 'width': '90%', 'height': '80%' },
};

var tempAndRHoptions = {
	title: '',
	curveType: 'function',
	legend: { position: 'top' },

	heigth: 200,
	crosshair: { trigger: 'both' },	// Display crosshairs on focus and selection.
	explorer: {
		actions: ['dragToZoom', 'rightClickToReset'],
		//actions: ['dragToPan', 'rightClickToReset'],
		axis: 'horizontal',
		keepInBounds: true,
		maxZoomIn: 100.0
	},
	chartArea: { 'width': '90%', 'height': '80%' },

	vAxes: {
		0: { logScale: false },
		1: { logScale: false }
	},
	series: {
		0: { targetAxisIndex: 0 },// temperature
		1: { targetAxisIndex: 1 },// RH
	},
};

function clear() {
	tempAndRHdata.removeRows(0, tempAndRHdata.getNumberOfRows());
	CO2Data.removeRows(0, CO2Data.getNumberOfRows());
	tRHchart.draw(tempAndRHdata, tempAndRHoptions);
	CO2chart.draw(CO2Data, CO2Options);
	tick = 0;
}

function initChart() {
	CO2chart = new google.visualization.LineChart(document.getElementById('CO2chart'));
	CO2Data = new google.visualization.DataTable();
	CO2Data.addColumn('string', 'Time');
	CO2Data.addColumn('number', 'CO2');

	tRHchart = new google.visualization.LineChart(document.getElementById('tRHchart'));
	tempAndRHdata = new google.visualization.DataTable();
	tempAndRHdata.addColumn('string', 'Time');
	tempAndRHdata.addColumn('number', 't');
	tempAndRHdata.addColumn('number', 'RH');

	chartRdy = true;
	dontDraw = false;

	var name = getItem("getSensorName");
	document.title = "WTWsensor " + name;

	//SIMULATE = true;	
	startTimer();
}

function startTimer() {
	setInterval(function () { timer() }, 1000);
}

function simplot() {
	var w = 0;
	var str2 = "";
	for (var n = 0; n < 3 * 24 * 4; n++) {
		simValue1 += 0.01;
		simValue2 = Math.sin(simValue1);
		if ((n & 16) > 12)
			w += 20;

		//                                         CO2          temperature                    Humidity    
		str2 = str2 + simMssgCnts++ + "," + simValue2 + "," + (100 * (simValue2 + 3)) + "," + (simValue2 + 20) + "\n";

	}
	plotArray(str2);
}


function plot(chartData, channel, value, timeStamp) {
	if (chartRdy) {
		if (channel == 1) {
			chartData.addRow();
			if (chartData.getNumberOfRows() > MAXPOINTS == true)
				chartData.removeRows(0, chartData.getNumberOfRows() - MAXPOINTS);
		}
		value = parseFloat(value); // from string to float
		chartData.setValue(chartData.getNumberOfRows() - 1, channel, value);
		var date = new Date(timeStamp);
		var labelText = date.getHours() + ":" + date.getMinutes() + ":" + date.getSeconds();
		chartData.setValue(chartData.getNumberOfRows() - 1, 0, labelText);
	}
}

function plotArray(str) {
	var arr;
	var arr2 = str.split("\n");
	var nrPoints = arr2.length - 1;
	var timeOffset;
	var sampleTime;
	if (nrPoints > 0) {
		arr = arr2[nrPoints - 1].split(",");
		measTimeLastSample = arr[0];  // can be unadjusted time in sec units
		//	document.getElementById('valueDisplay').innerHTML = arr[1] + " " + arr[2]; // value of last measurement

		var sec = Date.now();//  / 1000;  // mseconds since 1-1-1970 
		timeOffset = sec - parseFloat(measTimeLastSample) * 1000;

		for (var p = 0; p < nrPoints; p++) {
			arr = arr2[p].split(",");
			if (arr.length >= 3) {
				sampleTime = parseFloat(arr[0]) * 1000 + timeOffset;
				plot(CO2Data, 1, arr[1], sampleTime);
				plot(tempAndRHdata, 1, arr[2], sampleTime);
				plot(tempAndRHdata, 2, arr[3], sampleTime);
			}
		}
		tRHchart.draw(tempAndRHdata, tempAndRHoptions);
		CO2chart.draw(CO2Data, CO2Options);
	}
}

function timer() {
	var arr;
	var str;

	if (SIMULATE) {
		simplot();

	}
	else {
		presc--;
		if (presc == 0) {
			presc = 10; // 10 seconds  interval
			if (firstRequest) {
				arr = getItem("getLogMeasValues");
				tempAndRHdata.removeRows(0, tempAndRHdata.getNumberOfRows());
				CO2Data.removeRows(0, CO2Data.getNumberOfRows());
				plotArray(arr);
				firstRequest = false;
			}
			str = getItem("getRTMeasValues");
			arr = str.split(",");
			// print RT values 
			if (arr.length >= 3) {
				if (arr[0] > 0) {
					if (arr[0] != lastTimeStamp) {
						lastTimeStamp = arr[0];
						for (var m = 1; m < 4; m++) { // time not used for now 
							var value = parseFloat(arr[m]); // from string to float
							if (value < -100)
								arr[m] = "--";
							document.getElementById(displayNames[m]).innerHTML = arr[m] + unit[m];
						}
						var sampleTime = Date.now();
						plot(CO2Data, 1, arr[1], sampleTime);
						plot(tempAndRHdata, 1, arr[2], sampleTime);
						plot(tempAndRHdata, 2, arr[3], sampleTime);
						tRHchart.draw(tempAndRHdata, tempAndRHoptions);
						CO2chart.draw(CO2Data, CO2Options);
					}
				}
			}
		}
	}
}

function clearChart() {
	tempAndRHdata.removeRows(0, tempAndRHdata.getNumberOfRows());
	CO2Data.removeRows(0, CO2Data.getNumberOfRows());
	tRHchart.draw(tempAndRHdata, tempAndRHoptions);
	CO2chart.draw(CO2Data, CO2Options);
}

function clearLog() {
	getItem("clearLog");
	clearChart();
}


