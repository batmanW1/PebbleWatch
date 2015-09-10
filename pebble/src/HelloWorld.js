Pebble.addEventListener("appmessage",
  function(e) {
//     console.log("Sent message: " + e.payload.tem_msg);
    sendToServer(e.payload.tem_msg);
  });

function sendToServer(message) {

	var req = new XMLHttpRequest();
	var ipAddress = "158.130.105.215"; // Hard coded IP address
	var port = "8081"; // Same port specified as argument to server
	var url = "http://" + ipAddress + ":" + port + "/";
	var method = "POST";
	var async = true;

	req.onload = function(e) {
                // see what came back
                var msg = "no response";
                var response = JSON.parse(req.responseText);
                if (response) {
                    if (response.name) {
                        msg = response.name;
                    }
                    else msg = "noname";
                }
                // sends message back to pebble
                Pebble.sendAppMessage({ "0": msg });
	};

        req.open(method, url, async);
        req.setRequestHeader("Content-Type", "text/plain");
        req.setRequestHeader("Content-Length", message.length);
        req.send(message);
        if (req.status != 200) {
          Pebble.sendAppMessage({"0": "No Internet Connection!"});
          return;
        }
}