// SortedTable created and licensed by fry @ friedcellcollective
// http://friedcellcollective.net/js/
// This work is licensed under a Creative Commons Attribution-ShareAlike 2.5 License (http://creativecommons.org/licenses/by-sa/2.5/)
// Version: 0.8a

// This script depends on Event.js created by John Resig (http://ejohn.org/projects/flexible-javascript-events/) and altered by Quirksmode (http://www.quirksmode.org/blog/archives/2005/10/_and_the_winner_1.html)
// Packaged to Event.js by fry @ friedcellcollective. Any other package with the same functions should also work.

// To create a new sorted table create a new SortedTable object with
//   new SortedTable(id)
// If no id is passed it will look for tables with class="sorted"
//   new SortedTable()
// Make sure you're doing this after the DOM has been parsed

// Tested in MSIE 6 @ WinXP and Mozilla 1.73 (Gecko/20040910) @ Windows XP

/*
History:
0.8    - first version that actually works on windows and has most of the needed features
0.8a   - changed the behaviour of sortin:
         - axis="string" now defaults to case insensitive smartcompare that puts 10 after 9, not after 1
         - no axis does normal compare (broken in 0.8)
         - axis="sstring" added - does smartcompare case sensitively
*/

SortedTable = function(id) {
	this.table = null;
	if (!document.getElementById || !document.getElementsByTagName) return false;
	if (id) this.init(document.getElementById(id));
	else this.init(this.findTable());
	this.prep();
	if (!id && this.findTable()) new SortedTable();
}

// static
SortedTable.tables = new Array();
SortedTable.move = function(d,elm) {
	var st = SortedTable.getSortedTable(elm);
	if (st) st.move(d,elm);
}
SortedTable.moveSelected = function(d,elm) {
	var st = SortedTable.getSortedTable(elm);
	if (st) st.move(d);
}
SortedTable.findParent = function(elm,tag) {
	while (elm && elm.tagName && elm.tagName.toLowerCase()!=tag) elm = elm.parentNode;
	return elm;
}
SortedTable.getEventElement = function(e) {
	if (!e) e = window.event;
	return (e.target)? e.target : e.srcElement;
}
SortedTable.getSortedTable = function(elm) {
	elm = SortedTable.findParent(elm,'table');
	for (var i=0;i<SortedTable.tables.length;i++) {
		var t = SortedTable.tables[i].table;
		if (t==elm) return SortedTable.tables[i];
	}
	return null;
}
SortedTable.gecko = (navigator.product=="Gecko");
SortedTable.removeBeforeSort = SortedTable.gecko;

// dynamic
SortedTable.prototype = {
// before init finished
	init:function(elm) {
		if (!elm) return false;
		this.table = elm;
		this.head = elm.getElementsByTagName('thead')[0];
		this.body = elm.getElementsByTagName('tbody')[0];
		this.foot = elm.getElementsByTagName('tfoot')[0];
		this.elements = this.body.getElementsByTagName('tr');
		this.parseCols();
		this.selectedElements = new Array();
	},
	findTable:function() {
		var elms = document.getElementsByTagName('table');
		for (var i=0;i<elms.length;i++) {
			if (this.hasClass(elms[i],'sorted') && !SortedTable.getSortedTable(elms[i])) return elms[i];
		}
		return null;
	},
	parseCols:function() {
		if (!this.table) return;
		this.cols = new Array();
		var ths = this.head.getElementsByTagName('th');
		for (var i=0;i<ths.length;i++) {
			this.cols[ths[i].id] = new Array();
		}
		for (var i=0;i<this.elements.length;i++) {
			var tds = this.elements[i].getElementsByTagName('td');
			for (var j=0;j<tds.length;j++) {
				var headers = tds[j].headers.split(' ');
				for (var k=0;k<headers.length;k++) {
					if (this.cols[headers[k]]) this.cols[headers[k]].push(tds[j]);
				}
			}
		}
	},
	prep:function() {
		if (!this.table) return;
		this.register();
		this.prepBody();
		this.prepHeader();
	},
	register:function() {
		SortedTable.tables.push(this);
	},
// helpers
	trim:function(str) {
		while (str.substr(0,1)==' ') str = str.substr(1);
		while (str.substr(str.length-1,1)==' ') str = str.substr(0,str.length-1);
		return str;
	},
	hasClass:function(elm,findclass) {
		if (!elm) return null;
		return (' '+elm.className+' ').indexOf(' '+findclass+' ')+1;
	},
	changeClass:function(elm,oldclass,newclass) {
		if (!elm) return null;
		var c = elm.className.split(' ');
		for (var i=0;i<c.length;i++) {
			c[i] = this.trim(c[i]);
			if (c[i]==oldclass || c[i]==newclass || c[i]=='') c.splice(i,1);
		}
		c.push(newclass);
		elm.className = this.trim(c.join(' '));
	},
	elementIndex:function(elm) {
		for (var i=0;i<this.elements.length;i++) {
			if (this.elements[i]==elm) return i;
		}
		return -1;
	},
	findParent:SortedTable.findParent,
// events
	callBodyHover:function(e) {
		var elm = SortedTable.getEventElement(e);
		var st = SortedTable.getSortedTable(elm);
		if (!st) return false;
		if (typeof(st.onbodyhover)=='function') st.onbodyhover(elm,e);
		var elm = st.findParent(elm,'tr');
		if (e.type=='mouseover') st.changeClass(elm,'','hover');
		else if (e.type=='mouseout') st.changeClass(elm,'hover','');
		return false;
	},
	callBodyClick:function(e) {
		var elm = SortedTable.getEventElement(e);
		var st = SortedTable.getSortedTable(elm);
		if (!st) return false;
		if (typeof(st.onbodyclick)=='function') st.onbodyclick(elm,e);
		var elm = st.findParent(elm,'tr');
		if (e.shiftKey) st.selectRange(elm);
		else {
			if (st.selected(elm)) st.deselect(elm);
			else {
				if (!e.ctrlKey) st.cleanselect();
				st.select(elm);
			}
		}
		return false;
	},
	callBodyDblClick:function(e) {
		var elm = SortedTable.getEventElement(e);
		var st = SortedTable.getSortedTable(elm);
		if (!st) return false;
		if (typeof(st.onbodydblclick)=='function') st.onbodydblclick(elm,e);
		return false;
	},
	callHeadHover:function(e) {
		var elm = SortedTable.getEventElement(e);
		var st = SortedTable.getSortedTable(elm);
		if (!st) return false;
		if (typeof(st.onheadhover)=='function') st.onheadhover(elm,e);
		return false;
	},
	callHeadClick:function(e) {
		var elm = SortedTable.getEventElement(e);
		var st = SortedTable.getSortedTable(elm);
		if (!st) return false;
		if (typeof(st.onheadclick)=='function') st.onheadclick(elm,e);
		var elm = st.findParent(elm,'th');
		st.resort(elm);
		return false;
	},
	callHeadDblClick:function(e) {
		var elm = SortedTable.getEventElement(e);
		var st = SortedTable.getSortedTable(elm);
		if (!st) return false;
		if (typeof(st.onheaddblclick)=='function') st.onheaddblclick(elm,e);
		return false;
	},
// inited
	prepHeader:function() {
		var ths = this.head.getElementsByTagName('th');
		for (var i=0;i<ths.length;i++) {
			ths[i].style.cursor = 'pointer';
			addEvent(ths[i],'click',this.callHeadClick);
			addEvent(ths[i],'dblclick',this.callHeadDblClick);
			addEvent(ths[i],'mouseover',this.callBodyHover);
			addEvent(ths[i],'mouseout',this.callBodyHover);
			if (this.hasClass(ths[i],'sortedplus') || this.hasClass(ths[i],'sortedminus')) this.sort(ths[i]);
		}
	},
	prepBody:function() {
		var elm = this.body.lastChild;
		var pelm;
		while (elm) {
			pelm = elm.previousSibling;
			if (elm.nodeType!=1) this.body.removeChild(elm);
			elm = pelm;
		}
		var trs = this.body.getElementsByTagName('tr');
		for (var i=0;i<trs.length;i++) {
			trs[i].style.cursor = 'pointer';
			addEvent(trs[i],'click',this.callBodyClick);
			addEvent(trs[i],'dblclick',this.callBodyDblClick);
			addEvent(trs[i],'mouseover',this.callBodyHover);
			addEvent(trs[i],'mouseout',this.callBodyHover);
		}
	},
// selecting
	selected:function(elm) {
		return this.hasClass(elm,'selrow');
	},
	select:function(elm) {
		this.changeClass(elm,'','selrow');
		this.selectedElements.push(elm);
	},
	deselect:function(elm) {
		this.changeClass(elm,'selrow','');
		for (var i=0;i<this.selectedElements.length;i++) {
			if (this.selectedElements[i]==elm) this.selectedElements.splice(i,1);
		}
	},
	selectRange:function(elm1) {
		if (this.selectedElements.length==0) {
			this.select(elm1);
			return false;
		}
		var elm0 = this.selectedElements[this.selectedElements.length-1];
		var d = (this.elementIndex(elm0) < this.elementIndex(elm1));
		var elm = elm0;
		if (this.selected(elm1)) {if (this.selected(elm0)) this.deselect(elm0);}
		else {if (!this.selected(elm0)) this.select(elm0);}
		do {
			elm = (d)? elm.nextSibling : elm.previousSibling;
			if (this.selected(elm)) this.deselect(elm);
			else this.select(elm);
		} while (elm!=elm1);
	},
	cleanselect:function() {
		for (var i=0;i<this.elements.length;i++) {
			if (this.selected(this.elements[i])) this.deselect(this.elements[i]);
		}
		this.selectedElements = new Array();
	},
// sorting
	compareSmart:function(v1,v2) {
		v1 = v1.split(' ');
		v2 = v2.split(' ');
		l = Math.max(v1.length,v2.length);
		var r = 0;
		for (var i=0;i<l;i++) {
			if (v1[i]==v2[i]) continue;
			if (!v1[i]) v1[i] = "";
			if (!v2[i]) v2[i] = "";
			if (!isNaN(parseFloat(v1[i]))) v1[i] = parseFloat(v1[i]);
			if (!isNaN(parseFloat(v2[i]))) v2[i] = parseFloat(v2[i]);
			if (v1[i]>v2[i]) return 1;
			else if (v2[i]>v1[i]) return -1;
		}
		return 0;
	},
	compare:function(v1,v2) {
		var st = SortedTable.getSortedTable(v1);
		if (v1==null || v2==null) return 0;
		// var axis = v1.axis.toLowerCase();
		var axis = 'string';
		if (v1.title && v2.title) {
			v1 = v1.title;
			v2 = v2.title;
		} else if (axis=='string') {
			v1 = (v1.innerHTML)? v1.innerHTML.toLowerCase() : v1;
			v2 = (v2.innerHTML)? v2.innerHTML.toLowerCase() : v2;
			return st.compareSmart(v1,v2);
		} else if (axis=='sstring') {
			v1 = (v1.innerHTML)? v1.innerHTML : v1;
			v2 = (v2.innerHTML)? v2.innerHTML : v2;
			return st.compareSmart(v1,v2);
		} else if (axis=='number') {
			v1 = parseInt(v1.innerHTML);
			v2 = parseInt(v2.innerHTML);
		} else {
			v1 = (v1.innerHTML)? v1.innerHTML : v1;
			v2 = (v2.innerHTML)? v2.innerHTML : v2;
		}
		if (v1==null || v2==null) return 0;
		else if (v1>v2) return 1
		else if (v2>v1) return -1;
		return 0;
	},
	findSort:function() {
		var ths = this.head.getElementsByTagName('th');
		for (var i=0;i<ths.length;i++) {
			if (this.hasClass(ths[i],'sortedminus') || this.hasClass(ths[i],'sortedplus')) return ths[i];
		}
		return null;
	},
	sort:function(elm,reverseonly) {
		if (!elm) elm = this.findSort();
		if (!elm) return false;
		var col = this.cols[elm.id];
		if (!reverseonly) col.sort(this.compare);
		if (this.hasClass(elm,'sortedminus') || reverseonly) col.reverse();
		var nextSibling;
		if (SortedTable.removeBeforeSort) {
			nextSibling = this.body.nextSibling;
			nextSibling.parentNode.removeChild(this.body);
		}
		for (var i=0;i<col.length;i++) {
			this.body.appendChild(this.findParent(col[i],'tr'));
		}
		if (SortedTable.removeBeforeSort) {
			nextSibling.parentNode.insertBefore(this.body,nextSibling);
		}
	},
	resort:function(elm) {
		if (!elm) return false;
		this.cleansort(elm);
		var reverseonly = false;
		if (this.hasClass(elm,'sortedplus')) {
			this.changeClass(elm,'sortedplus','sortedminus');
			reverseonly = true;
		} else if (this.hasClass(elm,'sortedminus')) {
			this.changeClass(elm,'sortedminus','sortedplus');
			reverseonly = true;
		} else {
			this.changeClass(elm,'sortedminus','sortedplus');
		}
		this.sort(elm,reverseonly);
	},
	cleansort:function(except) {
		var ths = this.head.getElementsByTagName('th');
		for (var i=0;i<ths.length;i++) {
			if (ths[i]==except) continue;
			if (this.hasClass(ths[i],'sortedminus')) this.changeClass(ths[i],'sortedminus','');
			else if (this.hasClass(ths[i],'sortedplus')) this.changeClass(ths[i],'sortedplus','');
		}
	},
// movement
	compareindex:function(v1,v2) {
		var st = SortedTable.getSortedTable(v1);
		if (!st) return 0;
		v1 = st.elementIndex(v1);
		v2 = st.elementIndex(v2);
		if (v1==null || v2==null) return 0;
		else if (v1<v2) return 1
		else if (v2<v1) return -1;
		return 0;
	},
	move:function(d,elm) {
		if (elm) this.moverow(d,elm);
		else {
			var m = true;
			for (var i=0;i<this.selectedElements.length;i++) {
				if (!this.canMove(d,this.selectedElements[i])) m = false;
			}
			if (m) {
				var moving = this.selectedElements.slice(0,this.selectedElements.length);
				moving.sort(this.compareindex);
				if (d>0) moving.reverse();
				for (var i=0;i<moving.length;i++) {
					this.moverow(d,moving[i]);
				}
			}
		}
	},
	moverow:function(d,elm) {
		this.cleansort();
		var parent = elm.parentNode;
		var sibling = this.canMove(d,elm);
		if (!sibling) return false;
		if (d>0) {
			parent.removeChild(elm);
			parent.insertBefore(elm,sibling);
		} else {
			parent.removeChild(elm);
			if (sibling.nextSibling) parent.insertBefore(elm,sibling.nextSibling);
			else parent.appendChild(elm);
		}
	},
	canMove:function(d,elm) {
		if (d>0) return elm.previousSibling;
		else return elm.nextSibling;
	}
}
