function weatherChart(category, align, series) {

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
	    series: series,
	    stroke: {curve: 'smooth'},
	    tooltip: {x: {format: "dd MMM yy, HH:mm"}},
	    dataLabels: {enabled: false}};
}

var hchart, cchart, tchart, pchart;

function init() {

    // create the charts
    tchart = new ApexCharts(document.querySelector("#temperature"),
			    weatherChart("Temperature", "left", []));
    cchart = new ApexCharts(document.querySelector("#clouds"),
			    weatherChart("Cloud Coverage", "right", []));
    pchart = new ApexCharts(document.querySelector("#pressure"),
			    weatherChart("Pressure", "left", []));
    hchart = new ApexCharts(document.querySelector("#humidity"),
			    weatherChart("Humidity", "right", []));

    // update the current value
    $.get("CHART/RTdata.json", function(data) {
	hchart.render();
	hchart.updateOptions({title: {text: (data.HR).toFixed(0) + "%"}});

	cchart.render();
	cchart.updateOptions({title: {text: (data.clouds).toFixed(0) + "%"}});

	tchart.render();
	tchart.updateOptions({title: {text: (data.T).toFixed(1) + "°C"}});

	pchart.render();
	pchart.updateOptions({title: {text: (data.P).toFixed(0) + " hPa"}});
    });

    updateSeries();
};

function updateSeries() {
    $.get("CHART/RTfulldata.json", function(data) {
	hchart.updateSeries([data.HR]);
	cchart.updateSeries([data.clouds]);
	tchart.updateSeries([data.T]);
	pchart.updateSeries([data.P]);
    });
};
