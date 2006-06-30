// This script created by John Resig (http://ejohn.org/projects/flexible-javascript-events/) and altered by Quirksmode (http://www.quirksmode.org/blog/archives/2005/10/_and_the_winner_1.html)
// Packaged to Event.js by fry @ friedcellcollective.

function addEvent(obj,type,fn) {
	if (obj.addEventListener) obj.addEventListener(type,fn,false);
	else if (obj.attachEvent)	{
		obj["e"+type+fn] = fn;
		obj[type+fn] = function() {obj["e"+type+fn](window.event);}
		obj.attachEvent("on"+type, obj[type+fn]);
	}
}

function removeEvent(obj,type,fn) {
	if (obj.removeEventListener) obj.removeEventListener(type,fn,false);
	else if (obj.detachEvent) {
		obj.detachEvent("on"+type, obj[type+fn]);
		obj[type+fn] = null;
		obj["e"+type+fn] = null;
	}
}