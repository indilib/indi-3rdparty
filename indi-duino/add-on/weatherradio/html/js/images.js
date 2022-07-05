function loadImages(width) {
    updateImageBar(width);
    updateCarousel();
    // update images every 5 min
    setInterval( function(){ loadImages(width);}, 5*60000);
}

function openLightbox(n) {
    lightboxContent = document.getElementById("lightbox-content");
    document.getElementById("lightbox").style.display = "block";

    // select the image from the series
    $("#timeseries").carousel(n);


}

function closeLightbox() {
  document.getElementById("lightbox").style.display = "none";
}

function updateCarousel() {
    $.get("data/images.json", function(files) {
	carouselInner      = document.querySelector("#timeseries-carousel");
	carouselIndicators = document.querySelector("#timeseries-indicators");
	// clear the carousel and indicators
	carouselInner.innerHTML = "";
	carouselIndicators.innerHTML = "";

	pos = 0
	n   = files.length - 1
	// add all images and indicators
	for (file of files) {
	    // images
	    var div = document.createElement("div");
	    div.setAttribute("class", "carousel-item");
	    var img = document.createElement("img");
	    img.setAttribute("id", file.name);
	    img.setAttribute("src", "./media/" + file.name);
	    img.setAttribute("width", "100%");
	    div.appendChild(img);

	    // add date as caption
	    var cap = document.createElement("div");
	    cap.setAttribute("class", "carousel-caption");
	    cap.insertAdjacentText("afterbegin", new Date(file.ctime*1000).toLocaleString());
	    div.appendChild(cap);

	    carouselInner.appendChild(div);
	    
	    // indicators
	    var li = document.createElement("li");
	    li.setAttribute("data-target", "#timeseries");
	    li.setAttribute("data-slide-to", pos);
	    carouselIndicators.appendChild(li);
	    // activate the first image
	    if (pos++ == 0) {
		li.setAttribute("class", "active");
		div.setAttribute("class", "carousel-item active");
	    }
	}

    });
};

function updateImageBar(width) {
    $.get("data/images.json", function(files) {
	imageBar = document.querySelector("#image_bar");
	// clear the image bar
	imageBar.innerHTML = "";

	nr = 0;
	for (file of files) {
	    // images
	    var img = document.createElement("img");
	    src = "./media/" + file.name;
	    img.setAttribute("id", file.name);
	    img.setAttribute("src", src);
	    img.setAttribute("width", width);

	    var link = document.createElement("a");
	    link.setAttribute("href", "#");
	    // select the image
	    link.setAttribute("onclick", "selectImage('" + src + "', " + nr++ + ")");
	    link.setAttribute("class", "hover-shadow");
	    link.setAttribute("title", new Date(file.ctime*1000).toLocaleString());
	    link.appendChild(img);
	    imageBar.appendChild(link);
	}

    });
};

function selectImage(image, nr) {
    img = document.getElementById("current_weather");
    img.setAttribute("src", image);
    link = document.getElementById("current_weather_link");
    link.setAttribute("onclick", "openLightbox(" + nr + ")");
};
