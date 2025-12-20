// グローバル変数
var map;
var marker;
var popupOptions = {
  'maxWidth': '500',
  'className': 'custom'
};
var editmode = false;
var currentLocationBtn = null;
var locating = false;
var deviceBaseUrl = null;

// ユーティリティ関数
function getUrlParameter(name) {
  var regexp = new RegExp('[?&]' + name + '=([^&#]*)', 'i');
  var output = regexp.exec(window.location.href);
  return output ? decodeURIComponent(output[1]) : null;
}

function setForm(lat, lng, zoom, comment) {
  var latField = document.getElementById('lat_input');
  var lngField = document.getElementById('lng_input');
  var zoomField = document.getElementById('zoom_input');
  latField.value = typeof lat === 'number' ? lat.toFixed(6) : lat;
  lngField.value = typeof lng === 'number' ? lng.toFixed(6) : lng;
  zoomField.value = typeof zoom === 'number' ? zoom : zoom;
  document.getElementById('comment_input').value = comment || '';
}

// デバイスAPI関連
function resolveDeviceBaseUrl() {
  if (deviceBaseUrl !== null) {
    return deviceBaseUrl;
  }
  var explicit = getUrlParameter('d');
  if (explicit) {
    var trimmed = explicit.trim();
    if (trimmed.length > 0) {
      if (/^https?:\/\//i.test(trimmed)) {
        deviceBaseUrl = trimmed.replace(/\/+$/, '');
      } else {
        deviceBaseUrl = ('http://' + trimmed).replace(/\/+$/, '');
      }
    } else {
      deviceBaseUrl = '';
    }
  } else {
    deviceBaseUrl = '';
  }
  return deviceBaseUrl;
}

function buildApiUrl(path) {
  var base = resolveDeviceBaseUrl();
  if (!base) {
    return path;
  }
  if (path.charAt(0) !== '/') {
    path = '/' + path;
  }
  return base + path;
}

// URL関連
function buildDefaultUrl() {
  var base = window.location.pathname || '/';
  var query = '?p=@35.681307,139.767015,17z&t=Tokyo%20Sta.';
  return base + query;
}

function buildShareUrl(lat, lng, zoom, comment) {
  var base = window.location.pathname || '/';
  var query = '?p=@' + lat + ',' + lng + ',' + zoom + 'z&t=' + encodeURIComponent(comment || '');
  return base + query;
}

function toAbsoluteUrl(url) {
  if (!url) {
    return '';
  }
  var origin = window.location.origin || (window.location.protocol + '//' + window.location.host);
  var absoluteUrl;
  try {
    absoluteUrl = new URL(url, origin).href;
  } catch (e) {
    return origin + (url.charAt(0) === '/' ? url : '/' + url);
  }
  // オープンリダイレクト対策: 同一オリジンのみ許可
  if (absoluteUrl.indexOf(origin) !== 0) {
    return origin + '/';
  }
  return absoluteUrl;
}

// 設定読み込み関連
function parseConfigFromQuery() {
  var p = getUrlParameter('p');
  if (!p) {
    return null;
  }
  var section = p.split('@');
  if (section.length < 2) {
    return null;
  }
  var coords = section[1].split(',');
  if (coords.length < 3) {
    return null;
  }
  var lat = parseFloat(coords[0]);
  var lng = parseFloat(coords[1]);
  var zoom = parseFloat(coords[2].replace('z', ''));
  if (!isFinite(lat) || !isFinite(lng) || !isFinite(zoom)) {
    return null;
  }
  if (lat < -90 || lat > 90 || lng < -180 || lng > 180) {
    return null;
  }
  var text = getUrlParameter('t') || '';
  // HTMLタグをエスケープしてXSSを防ぎ、改行文字を<br>に変換
  text = escapeHtmlWithLineBreaks(text);
  return { lat: lat, lng: lng, zoom: zoom, text: text };
}

// HTMLエスケープ関数（改行対応）
function escapeHtmlWithLineBreaks(text) {
  // まずHTMLタグをエスケープ
  var div = document.createElement('div');
  div.textContent = text;
  var escaped = div.innerHTML;
  // エスケープ後、改行文字を<br>に変換
  return escaped.replace(/\n/g, '<br>');
}

function loadDefaultConfig() {
  return { lat: 35.681307, lng: 139.767015, zoom: 17, text: 'Tokyo Sta.' };
}

function loadInitialConfig() {
  var queryConfig = parseConfigFromQuery();
  if (queryConfig) {
    return Promise.resolve(queryConfig);
  }
  return Promise.resolve(loadDefaultConfig());
}

// フォーム関連
function collectFormValues() {
  return {
    lat: document.getElementById('lat_input').value,
    lng: document.getElementById('lng_input').value,
    zoom: document.getElementById('zoom_input').value,
    comment: document.getElementById('comment_input').value || ''
  };
}

function submitLocationViaForm(payload, returnUrl) {
  var base = resolveDeviceBaseUrl();
  if (!base) {
    alert('d パラメータが不足しています。QRコードからページを開いてください。');
    return false;
  }
  
  // クエリパラメータを構築
  var params = new URLSearchParams();
  params.append('lat', payload.lat);
  params.append('lng', payload.lng);
  params.append('zoom', payload.zoom);
  params.append('text', payload.comment || '');
  if (returnUrl) {
    params.append('return', toAbsoluteUrl(returnUrl));
  }
  
  // GETリクエストとしてURLに遷移
  var apiUrl = buildApiUrl('/api/location') + '?' + params.toString();
  window.location.href = apiUrl;
  return true;
}

function updateUrl() {
  var payload = collectFormValues();
  var targetUrl = buildShareUrl(
    payload.lat,
    payload.lng,
    payload.zoom,
    payload.comment
  );
  if (editmode) {
    if (!submitLocationViaForm(payload, targetUrl)) {
      window.location.href = targetUrl;
    }
    return;
  }
  window.location.href = targetUrl;
}

// 位置情報取得関連
function handleLocationResult(newLat, newLng) {
  if (!map || !marker) {
    return;
  }
  map.setView([newLat, newLng], map.getZoom());
  marker.setLatLng([newLat, newLng]);
  document.getElementById('lat_input').value = newLat.toFixed(6);
  document.getElementById('lng_input').value = newLng.toFixed(6);
  document.getElementById('zoom_input').value = map.getZoom();
}

function setLocatingState(isLocating) {
  locating = isLocating;
  if (currentLocationBtn) {
    currentLocationBtn.classList.toggle('active', isLocating);
    currentLocationBtn.disabled = isLocating;
  }
}

// マップ初期化
function initializeMap(cfg) {
  map = L.map('map', {
    maxZoom: 18,
    minZoom: 6,
    zoomControl: false
  });
  new L.Control.Zoom({ position: 'bottomleft' }).addTo(map);
  map.setView([cfg.lat, cfg.lng], cfg.zoom);

  L.tileLayer('https://{s}.tile.osm.org/{z}/{x}/{y}.png', {
    maxZoom: 18,
    attribution: 'Map data &copy; <a href="http://openstreetmap.org">OpenStreetMap</a> contributors, '
  }).addTo(map);

  marker = L.marker([cfg.lat, cfg.lng]).addTo(map);
  // HTMLとして明示的に指定（デフォルトでもHTMLだが念のため）
  var popupContent = cfg.text || '';
  marker.bindPopup(popupContent, popupOptions).openPopup();

  map.on('move', function() {
    var pos = map.getCenter();
    var zoomLevel = map.getZoom();
    if (editmode && marker) {
      marker.setLatLng(pos);
    }
    document.getElementById('lat_input').value = pos.lat.toFixed(6);
    document.getElementById('lng_input').value = pos.lng.toFixed(6);
    document.getElementById('zoom_input').value = zoomLevel;
  });
}

// ページ初期化
function initPage() {
  $(window).on('load resize', function() {
    $('.map-main').outerHeight($(window).height());
  });

  // updateボタンのイベントリスナーを追加
  var updateBtn = document.getElementById('update_btn');
  if (updateBtn) {
    updateBtn.addEventListener('click', function(e) {
      e.preventDefault();
      updateUrl();
    });
  }

  currentLocationBtn = document.getElementById('currentLocationBtn');
  editmode = !!resolveDeviceBaseUrl();
  if (editmode) {
    $('#map-input').css('display', 'inline');
    if (currentLocationBtn) {
      currentLocationBtn.style.display = 'flex';
    }
  } else if (currentLocationBtn) {
    currentLocationBtn.style.display = 'none';
  }

  if (currentLocationBtn) {
    currentLocationBtn.addEventListener('click', function() {
      if (!editmode || locating) {
        return;
      }
      if (!navigator.geolocation) {
        alert('このブラウザは現在地の取得に対応していません。');
        return;
      }
      setLocatingState(true);
      navigator.geolocation.getCurrentPosition(function(position) {
        handleLocationResult(position.coords.latitude, position.coords.longitude);
        setLocatingState(false);
      }, function(error) {
        alert('現在地を取得できません: ' + error.message);
        setLocatingState(false);
      }, {
        enableHighAccuracy: true,
        timeout: 10000,
        maximumAge: 0
      });
    });
  }

  loadInitialConfig().then(function(cfg) {
    setForm(cfg.lat, cfg.lng, cfg.zoom, cfg.text);
    initializeMap(cfg);
  }).catch(function(err) {
    console.error('Failed to initialize map', err);
    window.location.href = buildDefaultUrl();
  });
}

// ページ読み込み完了後に初期化実行
initPage();
