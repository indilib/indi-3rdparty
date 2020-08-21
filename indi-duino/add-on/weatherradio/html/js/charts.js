function createWeatherChart(category, unit, align, max, precision) {

    var chart = {
        height: 250,
        type: "area",
        toolbar: {show: false},
    };
    var title = {
        text: category + " [" + unit.trim() + "]",
        align: align,
        offsetX: 6,
        offsetY: 15,
        style: {fontSize: '14px', color: '#ccc'}
    };
    var xaxis = {
        type: "datetime",
        labels: {style: {colors: '#ccc'}}
    };
    var yaxis = {
        labels: {style: {color: '#ccc'}},
        decimalsInFloat: precision,
        max: max
    };

    return {chart: chart,
            subtitle: title,
            xaxis: xaxis,
            yaxis: yaxis,
            series: [],
            stroke: {curve: 'smooth', width: 2},
            legend: {labels: {colors: ['#ccc']}},
            tooltip: {x: {format: "dd MMM yy, HH:mm"},
                      y: {formatter: function(value,
                                              {series, seriesIndex,
                                               dataPointIndex, w }) {
                          return value.toFixed(precision) + unit;}}},
            dataLabels: {enabled: false}};
}

function createSparklineWeatherChart(category) {

    return {
        chart: {
            height: 120,
            type: "area",
            sparkline: {enabled: true},
        },
        title: {
            text: category,
            align: 'center',
            offsetX: -10,
            offsetY: 10,
            style: {fontSize: '12px', color: '#ccc'}
        },
        series: [],
        xaxis: {type: "datetime"},
        stroke: {curve: 'smooth', width: 2},
        tooltip: {x: {format: "dd MMM yy, HH:mm"}}};
}

function createRadialBarChart(name, unit, min, max, precision) {
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
                    name: {show: true, offsetY: -15},
                    value: {
                        show: true,
                        offsetY: 0,
                        fontSize: "30px",
                        color: "#ccc",
                        formatter: function(val) {
                            return (min + (max - min) * val/100).toFixed(precision) + unit;
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
            height: 8,
            width: "67%",
            toolbar: {show: false},
            sparkline: {enabled: true}
        },
        plotOptions: {
            bar: {
                horizontal: true,
                colors: {
                    backgroundBarColors: ["#eee"],
                    backgroundBarOpacity: 0.1
                }
            }
        },
        dataLabels: {enabled: false},
        xaxis: {
            show: false,
            categories: [name],
            labels: {show: false},
            axisTicks: {show: false},
        },
        yaxis: {labels: {show: false}, min: min, max: max},
        tooltip: {enabled: false}
    });
}

function selectTimeline(activeElement, update) {
    var els = document.querySelectorAll("button");
    Array.prototype.forEach.call(els, function (el) {
        el.classList.remove('active');
    });

    activeElement.target.classList.add('active');

    // extract timeline from button ID
    var id = activeElement.target.id;
    if (id.indexOf("timeline_") >= 0) {
        timeline = id.substring(9, id.length);

        localStorage.setItem("timeline", timeline);

        update(timeline);
        currentTimeline = timeline;
    }
}


var hchart, cchart, tchart, pchart, schart, wchart, lchart;
var temperature, humidity, pressure, cloudCoverage, sqm, windSpeed;

var settings = {t_min: -40, t_max: 50, t_prec: 1,
                p_min: 973, p_max: 1053, p_prec: 0,
                sqm_min: 0, sqm_max: 25, sqm_prec: 1,
                windSpeed_min: 0, windSpeed_max: 35, windSpeed_prec: 1};

function init() {

    // add event listeners to buttons
    document.querySelector("#timeline_6h").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});
    document.querySelector("#timeline_1d").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});
    document.querySelector("#timeline_7d").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});
    document.querySelector("#timeline_30d").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});


    // create charts for current values
    temperature = new ApexCharts(document.querySelector("#temperature"),
                                 createRadialBarChart('Temperature', '°C',
                                                      settings.t_min, settings.t_max, settings.t_prec));
    humidity = new ApexCharts(document.querySelector("#humidity"),
                              createBarChart('Humidity', '%', 0, 100));
    pressure = new ApexCharts(document.querySelector("#pressure"),
                              createRadialBarChart('Pressure', ' hPa',
                                                   settings.p_min, settings.p_max, settings.p_prec));
    cloudCoverage = new ApexCharts(document.querySelector("#clouds"),
                                   createRadialBarChart('Cloud Coverage', '%', 0, 100, 0));
    sqm = new ApexCharts(document.querySelector("#sqm"),
                              createBarChart('SQM', '%', settings.sqm_min, settings.sqm_max));
    windSpeed = new ApexCharts(document.querySelector("#windSpeed"),
                              createBarChart('Wind Speed', '%', settings.windSpeed_min, settings.windSpeed_max));
    temperature.render();
    humidity.render();
    pressure.render();
    cloudCoverage.render();
    sqm.render();
    windSpeed.render();

    // special case for temperature
    temperature.updateOptions({
    }, true, false, false);
    
    // create the time series charts
    
    tchart = new ApexCharts(document.querySelector("#temperature_series"),
                            createWeatherChart("Temperature", "°C", "left", undefined, 1));
    hchart = new ApexCharts(document.querySelector("#humidity_series"),
                            createWeatherChart("Humidity", "%", "left", 100, 0));
    pchart = new ApexCharts(document.querySelector("#pressure_series"),
                            createWeatherChart("Pressure", "hPa", "left", undefined, 0));
    cchart = new ApexCharts(document.querySelector("#clouds_series"),
                            createWeatherChart("Cloud Coverage", "%", "left", 100, 0));
    schart = new ApexCharts(document.querySelector("#sqm_series"),
                            createWeatherChart("Sky Quality", "mag/arcsec²", "left", undefined, 1));
    wchart = new ApexCharts(document.querySelector("#windSpeed_series"),
                            createWeatherChart("Wind Speed", "m/s", "left", undefined, 1));

    hchart.render();
    cchart.render();
    tchart.render();
    pchart.render();
    schart.render();
    wchart.render();

    tchart.updateOptions({colors: ["#0077b3"],
                          fill: {type: ['gradient', 'pattern'],
                                 pattern: {style: 'verticalLines'}},
                          legend: {show: false}});

    wchart.updateOptions({colors: ["#ff0000", "#0077b3"],
                          legend: {show: false}});

    // select timeline
    document.querySelector("#timeline_" + getCurrentTimeline()).classList.add('active');

    updateSeries();

    // update timelines every 5 min
    setInterval( function(){ updateSeries(getCurrentTimeline());}, 5*60000);
};

function updateSeries() {
    // update last values
    $.get("data/RTdata_lastupdate.json", function(data) {

        var currentTemperature   = data.Temperature;
        var currentCloudCoverage = data.CloudCover;
        var currentHumidity      = data.Humidity;
        var currentPressure      = data.Pressure;
        var currentSQM           = data.SQM;
        var currentWindSpeed     = data.WindSpeed;
        var currentWindGust      = data.WindGust;

        // calculate filling percentage from current temperature and pressure (slightly ugly code)
	if (currentTemperature != null)
            temperature.updateSeries([100 * (currentTemperature - settings.t_min) / (settings.t_max - settings.t_min)]);
	if (currentPressure != null)
            pressure.updateSeries([100 * (currentPressure - settings.p_min) / (settings.p_max - settings.p_min)]);
	if (currentCloudCoverage != null)
            cloudCoverage.updateSeries([currentCloudCoverage]);
	if (currentHumidity != null) {
            humidity.updateSeries([{name: "Humidity", data: [currentHumidity]}]);
            document.querySelector("#humidityValue").textContent = currentHumidity.toFixed(0) + "%";
	}
	if (currentSQM != null) {
            sqm.updateSeries([{name: "SQM", data: [currentSQM]}]);
            document.querySelector("#sqmValue").textContent = currentSQM.toFixed(1);
	}
	if (currentWindSpeed != null) {
            windSpeed.updateSeries([{name: "Wind Speed", data: [currentWindSpeed]}]);
	    if (currentWindGust != null)
		document.querySelector("#windSpeedValue").textContent = currentWindSpeed.toFixed(1) + " m/s (max: " + currentWindGust.toFixed(1) + " m/s)";
	    else
		document.querySelector("#windSpeedValue").textContent = currentWindSpeed.toFixed(1) + " m/s";
	}

        // update time stamp at the bottom line
        var lastUpdate = data.timestamp;
        document.querySelector("#lastupdate").textContent = new Date(lastUpdate).toLocaleString();
    });

    timeline = getCurrentTimeline();

    $.get("data/RTdata_" + timeline + ".json", function(data) {

        hchart.updateSeries([data.Humidity]);
        cchart.updateSeries([data.CloudCover]);
        tchart.updateSeries([{data: data.Temperature.data,
                              name: data.Temperature.name,
                              type: "area"},
                             {data: data.DewPoint.data,
                              name: "Dew Point",
                              type: "area"}]);
        pchart.updateSeries([data.Pressure]);
        schart.updateSeries([data.SQM]);
        wchart.updateSeries([{data: data.WindGust.data,
                              name: "Wind Gust",
                              type: "line"},
                             {data: data.WindSpeed.data,
                              name: data.WindSpeed.name,
                              type: "area"}]);
    });

};

function initSensors() {

    // add event listeners to buttons
    document.querySelector("#timeline_6h").
        addEventListener('click', function (e) {selectTimeline(e, updateSensorSeries);});
    document.querySelector("#timeline_1d").
        addEventListener('click', function (e) {selectTimeline(e, updateSensorSeries);});
    document.querySelector("#timeline_7d").
        addEventListener('click', function (e) {selectTimeline(e, updateSensorSeries);});
    document.querySelector("#timeline_30d").
        addEventListener('click', function (e) {selectTimeline(e, updateSensorSeries);});


    // create the time series charts
    
    tchart = new ApexCharts(document.querySelector("#temperature_series"),
                            createWeatherChart("Temperature", " °C", "left", undefined, 1));
    hchart = new ApexCharts(document.querySelector("#humidity_series"),
                            createWeatherChart("Humidity", "%", "left", 100, 0));
    lchart = new ApexCharts(document.querySelector("#lum_series"),
                            createWeatherChart("Luminosity", " lux", "left", undefined, 0));

    tchart.render();
    hchart.render();
    lchart.render();

    // select timeline
    document.querySelector("#timeline_" + getCurrentTimeline()).classList.add('active');
    updateSensorSeries();
};
    
function updateSensorSeries(timeline) {

    timeline = getCurrentTimeline();

    $.get("data/RTsensors_" + timeline + ".json", function(data) {

        tchart.updateSeries([{data: data.BME280_Temp.data,
                              name: "Temp. (BME280)",
                              type: "area"},
                             {data: data.DHT_Temp.data,
                              name: "Temp. (DHT)",
                              type: "area"},
                             {data: data.MLX90614_Tamb.data,
                              name: "amb. Temp. (MLX90614)",
                              type: "area"},
                             {data: data.MLX90614_Tobj.data,
                              name: "obj. Temp. (MLX90614)",
                              type: "area"}]);
        hchart.updateSeries([{data: data.BME280_Hum.data,
                              name: "Hum. (BME280)",
                              type: "area"},
                             {data: data.DHT_Hum.data,
                              name: "Hum. (DHT)",
                              type: "area"}]);
        lchart.updateSeries([{data: data.TSL2591_Lux.data,
                              name: "Lum. (TSL2591)",
                              type: "area"}]);
    });
};


function getCurrentTimeline() {
    if (localStorage.getItem("timeline") == null)
        localStorage.setItem("timeline", "6h");
    
    return localStorage.getItem("timeline");
};
