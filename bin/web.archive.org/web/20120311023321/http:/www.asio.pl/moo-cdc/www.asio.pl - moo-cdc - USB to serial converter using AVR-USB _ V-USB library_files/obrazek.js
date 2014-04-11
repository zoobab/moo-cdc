




/*
     FILE ARCHIVED ON 21:22:50 Aug 13, 2013 AND RETRIEVED FROM THE
     INTERNET ARCHIVE ON 9:18:08 Apr 11, 2014.
     JAVASCRIPT APPENDED BY WAYBACK MACHINE, COPYRIGHT INTERNET ARCHIVE.

     ALL OTHER CONTENT MAY ALSO BE PROTECTED BY COPYRIGHT (17 U.S.C.
     SECTION 108(a)(3)).
*/
function pokazObrazek(picURL,picWidth,picHeight,picTitle) {
	newWindow=window.open(picURL,'newWin','toolbar=no,width='+picWidth+',height='+picHeight+',left=5,top=5');
	newWindow.document.write('<html><head><title>'+picTitle+'<\/title><\/head><body style="margin: 0px;"><img border="0" name="obrazek" src="'+picURL+'" onclick="javascript:window.close();" title="Zamknij Okno" alt="Zamknij Okno"><\/body><\/html>');
	newWindow.resizeBy(picWidth-newWindow.document.body.clientWidth,picHeight-newWindow.document.body.clientHeight);
	newWindow.document.close();
	newWindow.focus();
	return false;
}
