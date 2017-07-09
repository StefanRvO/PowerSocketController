$( document ).ajaxError(function( event, jqxhr, settings, exception ) {
    if ( jqxhr.status== 401 ) {
        window.location.replace("/login.html")
    }
});
