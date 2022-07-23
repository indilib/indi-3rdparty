function createWeatherChart(category, unit, align, min, max, precision) {

    var chart = {
        height: 250,
        type: "area",
        toolbar: {show: false},
    };
    var title = {
        text: category,
        align: align,
        offsetX: 0,
        offsetY: 0,
        style: {fontSize: '14px', color: '#ccc'}
    };
    var subtitle = {
        align: align,
        offsetX: 0,
        offsetY: 18,
        style: {fontSize: '24px', color: '#ccc'}
    };
    var xaxis = {
        type: "datetime",
        labels: {style: {colors: '#ccc'}}
    };
    var yaxis = {
        labels: {style: {color: '#ccc'}},
        decimalsInFloat: 0,
	min: min,
        max: max,
	// tickAmount: 6,
    };

    return {chart: chart,
	    title: title,
            subtitle: subtitle,
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

var settings = {t_min: -40, t_max: 50, t_prec: 1,
                p_min: 973, p_max: 1053, p_prec: 0,
                sqm_min: 0, sqm_max: 25, sqm_prec: 1,
                windSpeed_min: 0, windSpeed_max: 35, windSpeed_prec: 1};

function initCharts() {

    // add event listeners to buttons
    document.querySelector("#timeline_6h").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});
    document.querySelector("#timeline_1d").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});
    document.querySelector("#timeline_7d").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});
    document.querySelector("#timeline_30d").
        addEventListener('click', function (e) {selectTimeline(e, updateSeries);});


    // create the time series charts
    
    tchart = new ApexCharts(document.querySelector("#temperature_series"),
                            createWeatherChart("Temperature", "°C", "right", undefined, undefined, 1));
    hchart = new ApexCharts(document.querySelector("#humidity_series"),
                            createWeatherChart("Humidity", "%", "right", undefined, 100, 0));
    pchart = new ApexCharts(document.querySelector("#pressure_series"),
                            createWeatherChart("Pressure", " hPa", "right", undefined, undefined, 0));
    cchart = new ApexCharts(document.querySelector("#clouds_series"),
                            createWeatherChart("Cloud Coverage", "%", "right", undefined, 100, 0));
    schart = new ApexCharts(document.querySelector("#sqm_series"),
                            createWeatherChart("Sky Quality", " mag/arcsec²", "right", undefined, undefined, 1));
    wchart = new ApexCharts(document.querySelector("#windSpeed_series"),
                            createWeatherChart("Wind Speed", " m/s", "right", 0, undefined, 1));
    rvchart = new ApexCharts(document.querySelector("#rain_volume_series"),
                             createWeatherChart("Rain Volume", " l", "right", undefined, undefined, 1));
    richart = new ApexCharts(document.querySelector("#rain_intensity_series"),
                             createWeatherChart("Rain Intensity", " /min", "right", 0, undefined, 0));

    hchart.render();
    cchart.render();
    tchart.render();
    pchart.render();
    schart.render();
    wchart.render();
    rvchart.render();
    richart.render();

    tchart.updateOptions({colors: ["#0077b3"],
                          fill: {type: ['gradient', 'pattern'],
                                 pattern: {style: 'verticalLines'}},
                          legend: {show: false}});

    wchart.updateOptions({colors: ["#ff0000", "#0077b3"],
                          legend: {show: false}});

    rvchart.updateOptions({yaxis: {
	labels: {style: {color: '#ccc'}},
        decimalsInFloat: 1,
    }});

    // select timeline
    document.querySelector("#timeline_" + getCurrentTimeline()).classList.add('active');

    updateSeries();

    // update timelines every min
    setInterval( function(){ updateSeries(getCurrentTimeline());}, 1*60000);
};

function updateSeries() {
    // update image
    var weather_image = document.getElementById('current_weather');
    weather_image.src = 'media/current_weather.jpg?ts=' + Date.now();

    // update last values
    $.get("data/RTdata_lastupdate.json", function(data) {

        var currentTemperature   = data.Temperature;
        var currentCloudCoverage = data.CloudCover;
        var currentHumidity      = data.Humidity;
        var currentPressure      = data.Pressure;
        var currentSQM           = data.SQM;
        var currentWindSpeed     = data.WindSpeed;
        var currentWindGust      = data.WindGust;
        var currentRainVolume    = data.RainVolume;
        var currentRainIntensity = data.RaindropFrequency;

	if (currentTemperature != null)
	    tchart.updateOptions({subtitle: {text: currentTemperature.toFixed(1) + "°C"}});

	if (currentPressure != null)
	    pchart.updateOptions({subtitle: {text: currentPressure.toFixed(0) + " hPa"}});

	if (currentCloudCoverage != null)
	    cchart.updateOptions({subtitle: {text: currentCloudCoverage.toFixed(0) + "%"}});
	if (currentHumidity != null)
	    hchart.updateOptions({subtitle: {text: currentHumidity.toFixed(0) + "%"}});

	if (currentSQM != null)
	    schart.updateOptions({subtitle: {text: currentSQM.toFixed(1)}});

	if (currentWindSpeed != null)
	    wchart.updateOptions({subtitle: {text: currentWindSpeed.toFixed(1) + " m/s"}});

	if (currentRainVolume != null)
	    rvchart.updateOptions({subtitle: {text: currentRainVolume.toFixed(1) + " l"}});

	if (currentRainIntensity != null)
	    richart.updateOptions({subtitle: {text: currentRainIntensity.toFixed(0) + " /min"}});

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
        rvchart.updateSeries([data.RainVolume]);
        richart.updateSeries([data.RaindropFrequency]);
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
                            createWeatherChart("Temperature", " °C", "right", undefined, undefined, 1));
    hchart = new ApexCharts(document.querySelector("#humidity_series"),
                            createWeatherChart("Humidity", "%", "right", undefined, 100, 0));
    lchart = new ApexCharts(document.querySelector("#lum_series"),
                            createWeatherChart("Luminosity", " lux", "right", undefined, undefined, 0));

    tchart.render();
    hchart.render();
    lchart.render();

    // select timeline
    document.querySelector("#timeline_" + getCurrentTimeline()).classList.add('active');
    updateSensorSeries();
};
    
function updateSensorSeries(timeline) {

    timeline = getCurrentTimeline();

    // update last values
    $.get("data/RTsensors_lastupdate.json", function(data) {

        // update time stamp
        var lastUpdate = data.timestamp;
        document.querySelector("#lastupdate").textContent = new Date(lastUpdate).toLocaleString();
    });

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
