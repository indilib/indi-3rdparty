function createWeatherChart(category, align) {

    var offsetX = (align == 'left') ? 30 : 0;
    var chart = {
	height: 250,
	width: "95%",
	type: "area",
	toolbar: {show: false},
	animations: {
	    initialAnimation: {
		enabled: false
	    }
	}
    };
    var title = {
	text: "--",
	align: align,
	offsetX: offsetX,
	style: {fontSize: '24px', color: '#ccc'}
    };
    var subtitle = {
	text: category,
	align: align,
	offsetX: offsetX,
	style: {fontSize: '16px', color: '#ccc'}
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
	    subtitle: subtitle,
	    xaxis: xaxis,
	    yaxis: yaxis,
	    series: [],
	    stroke: {curve: 'smooth'},
	    tooltip: {x: {format: "dd MMM yy, HH:mm"}},
	    dataLabels: {enabled: false}};
}

var hchart, cchart, tchart, pchart;

function init() {

    // create the charts
    tchart = new ApexCharts(document.querySelector("#temperature"),
			    createWeatherChart("Temperature", "left"));
    cchart = new ApexCharts(document.querySelector("#clouds"),
			    createWeatherChart("Cloud Coverage", "right"));
    pchart = new ApexCharts(document.querySelector("#pressure"),
			    createWeatherChart("Pressure", "left"));
    hchart = new ApexCharts(document.querySelector("#humidity"),
			    createWeatherChart("Humidity", "right"));

    hchart.render();
    cchart.render();
    tchart.render();
    pchart.render();

    updateSeries();

    setInterval( function(){ updateSeries();}, 10000);
};

function updateSeries() {
    $.get("CHART/RTfulldata.json", function(data) {
	hchart.updateSeries([data.HR]);
	cchart.updateSeries([data.clouds]);
	tchart.updateSeries([data.T]);
	pchart.updateSeries([data.P]);

	// update last value
	tchart.updateOptions({title: {text: (data.T.data[data.T.data.length-1][1]).toFixed(0) + "°C"}});
	cchart.updateOptions({title: {text: (data.clouds.data[data.clouds.data.length-1][1]).toFixed(0) + "%"}});
	pchart.updateOptions({title: {text: (data.P.data[data.P.data.length-1][1]).toFixed(0) + " hPa"}});
	hchart.updateOptions({title: {text: (data.HR.data[data.HR.data.length-1][1]).toFixed(0) + "%"}});

    });
};
