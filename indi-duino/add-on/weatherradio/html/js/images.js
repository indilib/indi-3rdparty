function loadImages() {
    updateCarousel();
    // update images every 5 min
    setInterval( function(){ updateCarousel();}, 5*60000);
}


function updateCarousel() {
    $.get("data/images.json", function(files) {
	carouselInner      = document.querySelector("#timelapse_carousel");
	carouselIndicators = document.querySelector("#timelapse-indicators");
	// clear the carousel, remove existing DOM children
	for (child of carouselInner.children)
	    carouselInner.removeChild(child);
	// clear the carousel indicators, remove existing DOM children
	for (child of carouselIndicators.children)
	    carouselIndicators.removeChild(child);

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
	    div.appendChild(img);

	    // add date as caption
	    var cap = document.createElement("div");
	    cap.setAttribute("class", "carousel-caption");
	    cap.insertAdjacentText("afterbegin", new Date(file.ctime*1000).toLocaleString());
	    div.appendChild(cap);

	    carouselInner.appendChild(div);
	    
	    // indicators
	    var li = document.createElement("li");
	    li.setAttribute("data-target", "#timelapse");
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
