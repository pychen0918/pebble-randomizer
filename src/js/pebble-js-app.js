function xhrRequest(url, type, callback) {
	var xhr = new XMLHttpRequest();
	xhr.onload = function () {
		callback(this.responseText);
	};	
	xhr.open(type, url);
	xhr.send();
};

function locationSuccess(pos){
	console.log("Location success:" + 
		" lat=" + pos.coords.latitude +
		' lon=' + pos.coords.longitude +
		" alt=" + pos.coords.altitude +
		" posaccu=" + pos.coords.accuracy + 
		" altaccu=" + pos.coords.altitudeAccuracy +
		" head=" + pos.coords.heading +
		" speed=" + pos.coords.speed
		);
	var url = "https://maps.googleapis.com/maps/api/place/nearbysearch/json?" + 
		"location=" + pos.coords.latitude + "," + pos.coords.longitude + 
		"&radius=" + "500" + 
		"&types=" + "food" + 
		"&key=" + "AIzaSyDIRnvHHyGijXLZPAP9VZKb15EkB6oPI9s";
/*
	var url = "https://maps.googleapis.com/maps/api/place/nearbysearch/json?location=-33.8670522,151.1957362&radius=500&types=food&name=cruise&key=AIzaSyDIRnvHHyGijXLZPAP9VZKb15EkB6oPI9s";
*/
	console.log("url: " + url);

	xhrRequest(url, 'GET',
		function(responseText) {
			var json;
			var key;
			var dictionary = {};
			
			// Parse response with exception handle
			// It is possible that google returned with error page
			try{
				json = JSON.parse(responseText);
				// Parse and store the food info
				for(key in json.results){
					dictionary[key] = json.results[key].name;
					//console.log(key+" name: " + json.results[key].name);
					//console.log(key+" name: " + dictionary[key]);
				}
				console.log("List complete");
			} 
			catch(e){
				console.log("Error while parsing response: ");
				console.log(responseText);
				dictionary[0] = "Cannot get location information";
			}

			// Send to Pebble
			Pebble.sendAppMessage(dictionary,
				function(e) {
					console.log("List sent to Pebble successfully!");
				},
				function(e) {
					console.log("Error sending list info to Pebble!");
				}
			);
		}
	);
}

function locationError(err){
	console.log("Location error: code=" + err.code + " msg=" + err.message);
}


function getLocation() {
	navigator.geolocation.getCurrentPosition(
	locationSuccess,
	locationError,
	{enableHighAccuracy: false, timeout: 30000, maximumAge: 600000}
	);
}

Pebble.addEventListener("ready",
	function(e) {
		console.log("Ready");
	}
);

Pebble.addEventListener("appmessage",
	function(e) {
		console.log("App Message received");
		getLocation();
	}
);
