function createWeatherChart(category, align) {

    var chart = {
	height: 250,
	type: "area",
	toolbar: {show: false},
	animations: {
	    initialAnimation: {
		enabled: false
	    }
	}
    };
    var title = {
	text: category,
	align: align,
	style: {fontSize: '14px', color: '#ccc'}
    };
    var xaxis = {
	type: "datetime",
	labels: {style: {colors: '#ccc'}}
    };
    var yaxis = {
	labels: {style: {color: '#ccc'}}
    };

    return {chart: chart,
	    title: title,
	    xaxis: xaxis,
	    yaxis: yaxis,
	    series: [],
	    stroke: {curve: 'smooth', width: 1},
	    tooltip: {x: {format: "dd MMM yy, HH:mm"}},
	    dataLabels: {enabled: false}};
}

function createRadialBarChart(name, unit, min, max) {
    return ({
	chart: {
	    height: 300,
	    type: 'radialBar',
	},
	// set to 0
	series: [100 * (0 - min) / (max - min)],
	labels: [name],
	stroke: {
	    dashArray: 3
	},
	plotOptions: {
	    radialBar: {
		startAngle: -135,
		endAngle: 135,
		dataLabels: {
		    name: {show: true},
		    value: {
			show: true,
			fontSize: "30px",
			color: "#ccc",
			formatter: function(val) {
			    return parseInt(min + (max - min) * val/100) + unit;
			},
		    }
		},
		track: {show: true, opacity: 0.1},
	    }
	},
    });

}

function createBarChart(name, unit, min, max) {
    return ({
        series: [
	    {
		name: name,
		data: [50]
	    }],
        chart: {
	    type: 'bar',
	    height: 80,
	    width: "50%",
	    toolbar: {show: false},
	},
	offsetX: 0,
	offsetY: 0,
        plotOptions: {bar: {horizontal: true}},
        dataLabels: {
	    enabled: true,
	    style: {colors: ["#ccc"], fontSize: "14px"},
	    formatter: function(val) {
		return parseInt(val) + '%';
	    },
	},
	title: {
	    text: name,
	    align: "center",
	    offsetY: 0,
	    floating: true,
	    style: {color: '#ccc', fontSize: "14px"}
	},
        xaxis: {
	    show: false,
	    categories: ['Humidity'],
	    labels: {show: false},
	    axisTicks: {show: false},
	},
	yaxis: {labels: {show: false}, min: min, max: max},
	tooltip: {enabled: false}
    });

    
}

var hchart, cchart, tchart, pchart;
var temperature, humidity, pressure, cloudCoverage;

var settings = {t_min: -40, t_max: 50, p_min: 940, p_max: 990};

function init() {

    // create charts for current values
    temperature = new ApexCharts(document.querySelector("#temperature"),
				 createRadialBarChart('Temperature', '°C',
						      settings.t_min, settings.t_max));
    humidity = new ApexCharts(document.querySelector("#humidity"),
			      createBarChart('Humidity', '%', 0, 100));
    pressure = new ApexCharts(document.querySelector("#pressure"),
			      createRadialBarChart('Pressure', ' hPa',
						   settings.p_min, settings.p_max));
    cloudCoverage = new ApexCharts(document.querySelector("#clouds"),
				   createBarChart('Cloud Coverage', '%', 0, 100));
    temperature.render();
    humidity.render();
    pressure.render();
    cloudCoverage.render();
    
    // create the time series charts
    
    tchart = new ApexCharts(document.querySelector("#temperature_series"),
			    createWeatherChart("Temperature", "center"));
    hchart = new ApexCharts(document.querySelector("#humidity_series"),
			    createWeatherChart("Humidity", "center"));
    pchart = new ApexCharts(document.querySelector("#pressure_series"),
			    createWeatherChart("Pressure", "center"));
    cchart = new ApexCharts(document.querySelector("#clouds_series"),
			    createWeatherChart("Cloud Coverage", "center"));

    hchart.render();
    cchart.render();
    tchart.render();
    pchart.render();

    updateSeries();

    setInterval( function(){ updateSeries();}, 60000);
};

function updateSeries() {
    $.get("CHART/RTdata_6h.json", function(data) {
	hchart.updateSeries([data.HR]);
	cchart.updateSeries([data.clouds]);
	tchart.updateSeries([data.T]);
	pchart.updateSeries([data.P]);

	// update current value
	var currentTemperature   = Math.round(data.T.data[data.T.data.length-1][1]);
	var currentCloudCoverage = Math.round(data.clouds.data[data.clouds.data.length-1][1]);
	var currentHumidity      = Math.round(data.HR.data[data.HR.data.length-1][1]);
	var currentPressure      = Math.round(data.P.data[data.P.data.length-1][1])

	// calculate filling percentage from current temperature 
	temperature.updateSeries([100 * (currentTemperature - settings.t_min) / (settings.t_max - settings.t_min)]);
	humidity.updateSeries([{name: "Humidity", data: [currentHumidity]}]);
	pressure.updateSeries([100 * (currentPressure - settings.p_min) / (settings.p_max - settings.p_min)]);
	cloudCoverage.updateSeries([{name: "Cloud Coverage", data: [currentCloudCoverage]}]);

    });

    // update time stamp at the bottom line
    document.querySelector("#lastupdate").textContent = (new Date()).toLocaleString()
};
