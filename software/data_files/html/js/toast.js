function _(x) {return document.getElementById(x);}
class Toast {
    constructor() {
        this.snackbar = _("snackbar");
        this.snacktypes = {
            error: "isa_error",
            info: "isa_info",
            success: "isa_success",
            warning: "isa_warning"
        };
        this.snacktimeout = 3000;
    }
    snackShow(m, c) {
        if( m == "" || c == ""){ console.log('missing args!.'); }
        this.snackbar.innerHTML = m;
        this.snackbar.className = "snack " + this.snacktypes[c];
        setTimeout(function(){
            this.snackbar.className = this.snackbar.className.replace("snack", "");
        }, this.snacktimeout);
    }
}
