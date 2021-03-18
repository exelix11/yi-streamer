var jmuxer = new JMuxer({
    node: 'player',
    mode: 'video',
    flushingTime: 1000,
    fps: 20, // Is this right ?
    debug: false
});

var socket = new WebSocket("ws://" + window.location.host + ":8080/");
socket.binaryType = 'arraybuffer';

socket.onopen = function() {
    document.getElementById("player").play();
}

socket.onmessage = function(event) {
	var buffer = new Uint8Array(event.data);
	
	var len = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
	//console.log("received " + buffer.length + " " + len + " " + buffer[4]);

    jmuxer.feed({
        duration: 0, // No clue about this, 0 seems to give the best performances
        video: buffer.slice(5),
    });	
};

function Lowres()
{
    socket.send("L");
    socket.close();
    setTimeout(function(){
        location.reload(); 
     },5);
}

function Hires()
{
    socket.send("H");
    socket.close();
    setTimeout(function(){
        location.reload(); 
     },5);
}