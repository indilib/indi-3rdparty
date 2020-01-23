function weatherChart(lastValue, category, align, series) {

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
	text: lastValue,
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

    xaxis = {type: "datetime"};

    return {chart: chart,
	    title: title,
	    subtitle: subtitle,
	    xaxis: xaxis,
	    series: series};
}

function init() {

    $.get("CHART/RTdata.json", function(data) {
	var hchart = new ApexCharts(document.querySelector("#humidity"),
				    weatherChart(data.HR + "%",
						 'Humidity', 'left',
						 seriesHumidity));
	hchart.render();
	var cchart = new ApexCharts(document.querySelector("#clouds"),
				    weatherChart(data.clouds + "%",
						 'Cloud Coverage', 'right',
						 seriesCloudCoverage));
	cchart.render();
	var tchart = new ApexCharts(document.querySelector("#temperature"),
				    weatherChart(data.T + "°C",
						 'Temperature', 'left',
						 seriesHumidity));
	tchart.render();
	var pchart = new ApexCharts(document.querySelector("#pressure"),
				    weatherChart(data.P + " hPa",
						 'Pressure', 'right',
						 seriesCloudCoverage));
	pchart.render();
    });
};
