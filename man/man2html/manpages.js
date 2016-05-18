<script src='jquery-2.2.3.min.js'></script>
<script src='jquery.history.min.js'></script>
<script>
$(document).ready(function() {
	var History = window.History;

	History.Adapter.bind(window, 'statechange', function() {
		var state = History.getState();
		var url = state.url;

		url = url.replace(/\.html$/, '_body.html');
		$('#content').load(url);
	});

	$(document).on('click', '.async', function() {
		var url   = $(this).attr('href');
		var list  = $(this).text().split(' ');
		var title = list[list.length - 1];

		title += " | BitKeeper Documentation";

		History.pushState(null, title, url);
		url = url.replace(/\.html$/, '_body.html');
		$('#content').load(url, function() {
			$(window).scrollTop(0);
		});
		return false;
	});
});
</script>
