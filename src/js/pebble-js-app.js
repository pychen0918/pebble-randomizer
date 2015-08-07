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
	console.log("url: " + url);

	xhrRequest(url, 'GET',
		function(responseText) {
			var json = JSON.parse(responseText);
			var key;
			var dictionary = {};
			
			// Parse and store the food info
			for(key in json.results){
				dictionary[key] = json.results[key].name;
				//console.log(key+" name: " + json.results[key].name);
				console.log(key+" name: " + dictionary[key]);
			}
			console.log("List complete");

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
	{timeout: 15000, maximumAge: 60000}
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
