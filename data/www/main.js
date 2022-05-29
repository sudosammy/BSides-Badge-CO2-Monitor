var gauge1 = Gauge(
  document.getElementById("gauge1"), {
    max: 2500,
    dialStartAngle: 90,
    dialEndAngle: 0,
    label: function(value) {
      return value.toLocaleString(undefined, {maximumFractionDigits: 0});
    },
    value: 400,
    color: function(value) {
      if(value < 300) {
          return "#ffffff";
        }else if(value < 800) {
          return "#00bfff";
        }else if(value < 1200) {
          return "#ff9100";
        }else if(value < 1600) {
          return "#fa5020#"
        } else {
          return "#a1000b";
      }
    },
  }
);

var gauge2 = Gauge(
    document.getElementById("gauge2"), {
      max: 50,
      dialStartAngle: 90,
      dialEndAngle: 0,
      label: function(value) {
        return Math.round(value * 100) / 100;
      },
      value: 20,
      color: function(value) {
        if(value < 0) {
            return "#2219ff";
          }else if(value < 15) {
            return "#3381ff";
          }else if(value < 25) {
            return "#0ff23c";
          }else if(value < 30) {
            return "#f7c80a#"
          } else {
            return "#f74d0a";
        }
      },
    }
  );

var gauge3 = Gauge(
document.getElementById("gauge3"), {
    max: 100,
    dialStartAngle: 90,
    dialEndAngle: 0,
    label: function(value) {
      return Math.round(value * 100) / 100;
    },
    value: 50,
    color: function(value) {
      if(value < 0) {
          return "#ff911c";
        }else if(value < 25) {
          return "#b4fc68";
        }else if(value < 50) {
          return "#72c5f2";
        }else if(value < 75) {
          return "#5e6cff#"
        } else {
          return "#4287f5";
      }
    },
}
);

// run once to get initial values
getCO2Data();
getTemperatureData();
getHumidityData();

// Thanks: https://github.com/melkati/CO2-Gadget
setInterval(function () {
  getCO2Data();
}, 5000); // 5s update rate

setInterval(function () {
  getTemperatureData();
}, 30000); // 30s update rate

setInterval(function () {
  getHumidityData();
}, 30000); // 30s update rate

function getCO2Data() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if ((this.readyState == 4) && (this.status == 200)) {
      gauge1.setValueAnimated(this.responseText, 2);
    }
  };
  xhttp.open("GET", "/co2", true);
  xhttp.setRequestHeader("Cache-Control", "no-cache, no-store, max-age=0");
  xhttp.setRequestHeader("Pragma", "no-cache");
  xhttp.send();
}

function getTemperatureData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      gauge2.setValueAnimated(this.responseText, 2);
    }
  };
  xhttp.open("GET", "/temp", true);
  xhttp.setRequestHeader("Cache-Control", "no-cache, no-store, max-age=0");
  xhttp.setRequestHeader("Pragma", "no-cache");
  xhttp.send();
}

function getHumidityData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      gauge3.setValueAnimated(this.responseText, 2);
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.setRequestHeader("Cache-Control", "no-cache, no-store, max-age=0");
  xhttp.setRequestHeader("Pragma", "no-cache");
  xhttp.send();
}

// datatables
$(document).ready(function () {
  var table = $('#co2table').DataTable({
    "ajax": {
      "url": "/table",
      "dataSrc": "data"
    },
    stateSave: true,
    dom: 'Bfrtip',
    buttons: [
      'copy', 'csv'
    ]
  });

  setInterval( function () {
    table.ajax.reload();
  }, 60000); // 60s update rate
});
