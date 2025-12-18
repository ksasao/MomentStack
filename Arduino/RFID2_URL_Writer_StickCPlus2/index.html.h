#ifndef index_html
#define index_html
String html="\n\
<!DOCTYPE html>\n\
<html>\n\
  <head>\n\
    <meta charset=\"utf-8\">\n\
    <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n\
    <title>Map Viewer</title>\n\
    <link rel=\"stylesheet\" href=\"https://d19vzq90twjlae.cloudfront.net/leaflet-0.7.3/leaflet.css\" />\n\
 \n\
    <!-- reference to Leaflet JavaScript -->\n\
    <script src=\"https://d19vzq90twjlae.cloudfront.net/leaflet-0.7.3/leaflet.js\"></script>\n\
    <script src=\"https://code.jquery.com/jquery.min.js\"></script>\n\
    <script>\n\
      // URLを更新\n\
      function updateUrl(){\n\
        lat = document.getElementById('lat_input').value;\n\
        lng = document.getElementById('lng_input').value;\n\
        zoom = document.getElementById('zoom_input').value;\n\
        comment = document.getElementById('comment_input').value\n\
        window.location.href = \"/?p=@\" + lat + \",\" + lng + \",\" + zoom + \"z&t=\" + comment;\n\
      }\n\
      // URL引数を取得\n\
      function getUrlParameter(name) {\n\
          var regexp = new RegExp('[?&]' + name + '=([^&#]*)', 'i');\n\
          var output = regexp.exec(window.location.href);\n\
          return output ? decodeURIComponent(output[1]) : null;\n\
      }\n\
      // フォームの値を更新\n\
      function setForm(lat,lng,zoom,comment){\n\
        document.getElementById('lat_input').value = lat;\n\
        document.getElementById('lng_input').value = lng;\n\
        document.getElementById('zoom_input').value = zoom;\n\
        document.getElementById('comment_input').value = comment;\n\
      }\n\
    </script>\n\
    <style>\n\
      body {\n\
        margin: 0;\n\
        overflow: hidden;\n\
      }\n\
      .custom .leaflet-popup-tip,\n\
      .custom .leaflet-popup-content-wrapper {\n\
          background: #ffffff;\n\
          color: #503020;\n\
          font-size: 250%;\n\
      }\n\
      input {\n\
        font-size:18px;\n\
      }\n\
      #map-input{\n\
        position: absolute;\n\
        left: 0; top: 0;\n\
        width: 100%; height: 100vh;\n\
        background: rgba(100, 100, 100, .8);\n\
        z-index: 2147483647;\n\
        display: none;\n\
      }\n\
      </style>\n\
  </head>\n\
  <body>\n\
    <div class=\"map-main\" id=\"map\" style=\"width: 100%; height: 1000px\"></div>\n\
    <div id=\"map-input\" style=\"width: 100%; height: 2.2em; line-height: 2em\">\n\
      <input type=\"text\" id=\"comment_input\" value=\"Hello!\" style=\"width:150px;\"/>\n\
      <input type=\"hidden\" id=\"lat_input\" />\n\
      <input type=\"hidden\" id=\"lng_input\" />\n\
      <input type=\"hidden\" id=\"zoom_input\" />\n\
      <input type=\"submit\" value=\"update\" onClick=\"updateUrl()\" />\n\
    </div>    \n\
    <script type=\"text/javascript\">\n\
      // 地図を全画面表示(初回ロード時, リサイズ時)\n\
      $(window).on('load resize', function() {\n\
        $('.map-main').outerHeight($(window).height());\n\
      });\n\
\n\
      // URLのパース\n\
      var defaultUrl = \"/?p=@35.681307,139.767015,17z&t=Tokyo%20Sta.\";\n\
      var str = getUrlParameter('p');\n\
      if(str === null){\n\
        window.location.href = defaultUrl;\n\
      }\n\
      var arr = str.split('@')[1];\n\
      var arr = arr.split(',');\n\
      var lat = parseFloat(arr[0]);\n\
      var lng = parseFloat(arr[1]);\n\
      var zoom = parseFloat(arr[2]);\n\
      var text = getUrlParameter('t');\n\
      if (!lat || !lng || !zoom || lat < -90 || lat > 90 || lng < -180 || lng > 180) {\n\
        window.location.href = defaultUrl;\n\
      }\n\
      setForm(lat,lng,zoom,text);\n\
\n\
      // 入力画面の表示・非表示\n\
      var edit = getUrlParameter('edit');\n\
      var editmode = false; // editmode は下のほうのマーカーの移動でも参照する\n\
      if(edit){\n\
        editmode = true;\n\
      }\n\
      if(editmode){ // CSSでは 'display: none' に設定されていることを期待\n\
        $('#map-input').css('display','inline');\n\
      }\n\
\n\
      //緯度,経度,ズームの表示設定\n\
      var map = L.map('map',{\n\
        maxZoom: 18,\n\
        minZoom: 6,\n\
        zoomControl: false\n\
      });\n\
      new L.Control.Zoom({ position: 'bottomleft' }).addTo(map);\n\
      map.setView([lat, lng], zoom);\n\
\n\
      // OpenStreetMap から地図画像を読み込む\n\
      L.tileLayer('https://{s}.tile.osm.org/{z}/{x}/{y}.png', {\n\
        maxZoom: 18,\n\
        attribution: 'Map data &copy; <a href=\"http://openstreetmap.org\">OpenStreetMap</a> contributors, '\n\
      }).addTo(map);\n\
\n\
      // ポップアップの表示\n\
      var popup = L.popup();\n\
      marker = L.marker([lat, lng],{}).addTo(map);\n\
      var customOptions =\n\
      {\n\
        'maxWidth': '500',\n\
        'className' : 'custom'\n\
      }\n\
      marker.bindPopup(text,customOptions).openPopup();\n\
\n\
      // マップの移動\n\
      map.on('move', function(e) {\n\
        var pos = map.getCenter();\n\
        if(editmode){\n\
          marker.setLatLng(pos);\n\
        }\n\
        var zoom = map.getZoom();\n\
        var lat = pos.lat.toFixed(6);\n\
        var lng =  pos.lng.toFixed(6);\n\
        document.getElementById('lat_input').value = lat;\n\
        document.getElementById('lng_input').value = lng;\n\
        document.getElementById('zoom_input').value = zoom;\n\
      });\n\
    </script>\n\
  </body>\n\
</html>\n\
";
#endif
