function xhrRequest(url, type, callback) {
	var xhr = new XMLHttpRequest();
	xhr.onload = function () {
		callback(this.responseText);
	};	
	xhr.open(type, url);
	xhr.send();
};

function getDistance(lat1, lon1, lat2, lon2) {
// Calculate distance by using approximation
	var R = 6731009;  // earth radius in meters
	var a = (lat1 - lat2)*(Math.PI / 180);
	var b = Math.cos((lat1+lat2)*(Math.PI/180)/2) * (lon1 - lon2)*(Math.PI/180);

	return Math.round((R * Math.sqrt( a*a + b*b ))/10)*10;  // roundup the number to 10 meters
}

function getDirection(lat1, lon1, lat2, lon2) {
// Calculate direction. 
// (lat1, lon1) is current position, and (lat2, lon2) is target position
	var directionName = ["N", "NE", "E", "SE", "S", "SW", "W", "NW", "N"];
	var t = Math.atan2(lon2-lon1, lat2-lat1)*(180/Math.PI);
	var index = Math.round(t / 45);
	if(index < 0)
		index = index+8;

	return directionName[index];
}

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
			var distance, lat, lon, direction;
			
			// Parse response with exception handle
			// It is possible that google returned with error page
			try{
				json = JSON.parse(responseText);
				// Parse and store the food info (name, distance, direction)
				for(key in json.results){
					lat = json.results[key].geometry.location.lat;
					lon = json.results[key].geometry.location.lng;
					distance = getDistance(pos.coords.latitude, pos.coords.longitude, lat, lon);
					direction = getDirection(pos.coords.latitude, pos.coords.longitude, lat, lon);

					console.log(key + 
						    " name: " + json.results[key].name + 
						    " distance: " + distance + 
						    " direction: " + direction);
					dictionary[key] = json.results[key].name;
					//console.log(key+" name: " + dictionary[key]);
				}
				console.log("List complete");
			} 
			catch(e){
				console.log("Error while parsing response: ");
				console.log(responseText);
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
	var dictionary = {};
	// timeout, send empty dictionary back
	console.log("Location error: code=" + err.code + " msg=" + err.message);
	Pebble.sendAppMessage(dictionary,
		function(e) {
			console.log("List sent to Pebble successfully!");
		},
		function(e) {
			console.log("Error sending list info to Pebble!");
		}
	);
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
