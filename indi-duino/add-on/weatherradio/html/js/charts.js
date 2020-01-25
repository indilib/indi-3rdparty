function weatherChart(category, align, series) {

    var offsetX = (align == 'left') ? 30 : 0;
    var chart = {
	height: 300,
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
	   dataLabels: {enabled: false}};
}

var hchart, cchart, tchart, pchart;

function init() {

    // create the charts
    hchart = new ApexCharts(document.querySelector("#humidity"),
			    weatherChart("Humidity", "left", []));
    cchart = new ApexCharts(document.querySelector("#clouds"),
			    weatherChart("Cloud Coverage", "right", []));
    tchart = new ApexCharts(document.querySelector("#temperature"),
			    weatherChart("Temperature", "left", []));
    pchart = new ApexCharts(document.querySelector("#pressure"),
			    weatherChart("Pressure", "right", []));

    // update the current value
    $.get("CHART/RTdata.json", function(data) {
	hchart.render();
	hchart.updateOptions({title: {text: data.HR + "%"}});

	cchart.render();
	cchart.updateOptions({title: {text: data.clouds + "%"}});

	tchart.render();
	tchart.updateOptions({title: {text: data.T + "°C"}});

	pchart.render();
	pchart.updateOptions({title: {text: data.P + " hPa"}});
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
