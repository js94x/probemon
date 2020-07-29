$(function() {
  var _chart = null, _rssi = null, _ds = null, _mac = null, _firstseen = null, _lastseen = null, _can_refresh = false;
  var tableau = ['#1f77b4', '#ff7f0e', '#2ca02c', /*'#d62728',*/ '#9467bd',
    '#8c564b', '#e377c2', /*'#7f7f7f',*/ '#bcbd22', '#17becf'];

  function median(values){
    if (values.length === 0) return 0;
    values.sort(function(a,b){
      return a-b;
    });
    var half = Math.floor(values.length / 2);
    if (values.length % 2) {
      return values[half];
    } else {
      return (values[half - 1] + values[half]) / 2.0;
    }
  }
  function* colorGenerator() {
    var c = 0;
    while (true) {
      yield tableau[c%tableau.length];
      c += 1;
    }
  }

  // https://stackoverflow.com/a/41491220/283067
  function pickTextColorBasedOnBgColorSimple(bgColor, lightColor, darkColor) {
    var color = (bgColor.charAt(0) === '#') ? bgColor.substring(1, 7) : bgColor;
    var r = parseInt(color.substring(0, 2), 16); // hexToR
    var g = parseInt(color.substring(2, 4), 16); // hexToG
    var b = parseInt(color.substring(4, 6), 16); // hexToB
    return (((r * 0.299) + (g * 0.587) + (b * 0.114)) > 186) ? darkColor : lightColor;
  }
  function updateDBTime() {
    $.ajax({
      url: '/api/stats/timestamp',
      dataType: 'json',
    }).done(function(data) {
      $('#dbtime').text(data.timestamp).css('text-decoration', '');
    }).fail(function(jq, status, error){
      $('#dbtime').css('text-decoration', 'line-through');
    });
  }

  function compare_rssi(a, b) {
    // sort by rssi number values and put LAA at the end with merged mac
    var xa = 3; xb = 3;
    if (a.label == "LAA") xa = 2;
    if (b.label == "LAA") xb = 2;
    if (a.label.length == 8) xa = 1;
    if (b.label.length == 8) xb = 1;
    if (xa > xb) return -1;
    if (xa < xb) return 1;
    if (a.data.length > b.data.length) return -1;
    if (a.data.length < b.data.length) return 1;
    return 0;
  }

  function ts_bounds() {
    if (_ds == null) return null;
    var first_seen = Number.MAX_SAFE_INTEGER;
    var last_seen = 0;
    for (let p of _ds) {
      // assuming data sorted by timestamp
      first_seen = Math.min(first_seen, p.data[0].x);
      last_seen = Math.max(last_seen, p.data[p.data.length-1].x);
    }
    return {'first_seen': first_seen, 'last_seen': last_seen};
  }

  function update_data(data, new_start_ts) {
    // trim unneeded timestamp data
    for (let i=0; i<_ds.length; i++) {
      var cd = _ds[i].data;
      var n = 0
      while (n < cd.length && cd[n].x < new_start_ts) {
        n++;
      }
      cd.splice(0, n);
    }
    for (let d of data) {
      // look for that mac in _ds
      var found = false, indx = null;
      for (let i=0; i<_ds.length; i++) {
        if (_ds[i].label == d.getMac()) {
          found = true;
          indx = i;
          break;
        }
      }
      if (found) {
        var ssids = [];
        for (let s of d.getSsidsList()) {
          var ssid = s.getName();
          ssids.push(ssid);
          if (_ds[indx].ssids.indexOf(ssid) == -1) {
            _ds[indx].ssids.push(ssid);
          }
        }
        var y = _ds.length;
        if (_ds[indx].data.length != 0) {
          y = _ds[indx].data[0].y;
        }
        var starting_ts = d.getStartingTs();
        var ns = [];
        for (let p of d.getProbereqList()) {
          // fix ssid number
          var si = _ds[indx].ssids.indexOf(ssids[p.getSsid()]);
          var q = {'x': p.getTimestamp()+starting_ts, 'y': y, 'rssi': p.getRssi(), 'ssid': si};
          ns.push(q);
        }
        // append new data
        _ds[indx].data = _ds[indx].data.concat(ns);
      } else {
        var ssids = [];
        for (let s of d.getSsidsList()) {
          ssids.push(s.getName());
        }
        var s = {
          label: d.getMac(),
          vendor: d.getVendor(),
          ssids: ssids,
          known: d.getKnown(),
          data: [],
          fill: false,
          pointStyle: 'line',
          pointRotation: 90,
          pointBorderColor: color,
          showLine: true,
          borderWidth: 1,
        };
        var starting_ts = d.getStartingTs();
        for (let p of d.getProbereqList()) {
          s.data.push({'x': p.getTimestamp()+starting_ts, 'y': _ds.length, 'rssi': p.getRssi(), 'ssid': p.getSsid()});
        }
        _ds.push(s);
      }
    }
    // remove chart with no timestamp anymore
    var empty = [];
    for (let i=0; i<_ds.length; i++) {
      if (_ds[i].data.length == 0) {
        empty.push(i);
      }
    }
    empty.reverse();
    for (let i of empty) {
      _ds.splice(i, 1);
    }
    // sort _ds on rssi number values; put LAA and merged at the end
    _ds.sort(compare_rssi);
    // fix y coord and diameter and color
    var gen = colorGenerator(), color;
    var diameter = ($('#main-chart').height()-50)/_ds.length-6;
    for (let i=0; i<_ds.length; i++) {
      _ds[i].pointRadius = diameter/2;
      _ds[i].pointHoverRadius = diameter/2;
      if (_ds[i].known) {
        color = '#d62728';
      } else if (_ds[i].label == 'LAA') {
        color = '#7f7f7f';
      } else {
        color = gen.next().value;
      }
      _ds[i].pointBorderColor = color;
      _ds[i].pointHoverRadius = color;
      _ds[i].pointHoverBorderColor = color;
      _ds[i].pointHoverBackgroundColor = color;
      _ds[i].backgroundColor = color;
      _ds[i].borderColor = color+'44';
      for (let t of _ds[i].data) {
        t.y = _ds.length-1-i;
      }
    }
  }

  function drawRSSIChart(dataset, color) {
    var ctx = document.getElementById('RSSI').getContext('2d');
    var ds = [];
    for (let p of dataset) {
        ds.push({t: p.x, y:p.rssi});
    }
    var radius = 2;
    if (ds.length > 50) {
      radius = 1;
    }
    var points = {
      label: dataset.label,
      data: ds,
      fill: false,
      showLine: false,
      pointRadius: radius,
      pointBorderColor: color,
      pointBackgroundColor: color,
    }
    if (_rssi === null) {
      _rssi = new Chart(ctx, {
        type: 'line',
        data: {datasets: []},
        options: {
          legend: {display: false, },
          tooltips: {enabled: false, },
          animation: { duration: 0 },
          scales: {
            xAxes: [{
              ticks: {fontColor: '#000', padding: 5, fontSize: 12},
              gridLines: { zeroLineColor: '#000', color: '#ccc', tickMarkLength: 5},
              type: 'time',
              time: {
                unit: 'hour',
                stepSize: 1,
                displayFormats: { hour: 'HH[h]', },
              }
            }],
          },
        },
      });
    }
    _rssi.data.datasets = [points];
    _rssi.options.scales.xAxes[0].time.min = _firstseen;
    _rssi.options.scales.xAxes[0].time.max = _lastseen;
    _rssi.update();
    $('#main-modal').modal('handleUpdate');
  }

  function updateRSSIModal(d) {
    if ((d === null) || (typeof d === 'undefined')) {
      return;
    }
    // reset mouse cursor to default
    $('html,body').css('cursor','default');
    // collapse possible collapsed toggle
    $('#collapseLog').collapse('hide');

    var mac = d.label;
    $('#main-modal').modal('show');
    // don't do any calculation if the data are already there
    if (mac == _mac) {
      return true;
    }
    _mac = mac;
    $('#mac').text(mac).css('color', d.pointBorderColor);
    $('#vendor').text(d.vendor);
    // stats
    var pr = d.data;
    var min = 100, max = -100, avg = 0, rssis = [], html = '';
    $('#mac_probes').empty();
    var count = 0;
    for (let p of pr) {
      min = Math.min(min, p.rssi);
      max = Math.max(max, p.rssi);
      avg += p.rssi;
      rssis.push(p.rssi);
      // logs
      if (count != 0 && count % 100 == 0) {
        var start = count - 100;
        var end = count -1;
        html += '<tr><td colspan="3"><a href="#" class="link-data" data-range="'+start+'-'+end+'">'+'Click to see range '+start+'-'+end+'</a></td></tr>';
      }
      count += 1;
    }
    count -= 1;
    if (count % 100 > -1) {
      var start = count - (count % 100);
      var end = count;
      html += '<tr><td colspan="3"><a href="#" class="link-data" data-range="'+start+'-'+end+'">'+'Click to see range '+start+'-'+end+'</a></td></tr>';
    }
    var ssids = [...d.ssids];
    if (ssids.indexOf('') > -1) {
      ssids.splice(ssids.indexOf(''), 1);
    }
    $('#ssids').text(ssids.join(', ')||'<none>');
    $('#count').text(pr.length);
    $('#min').text(min);
    $('#max').text(max);
    $('#avg').text((avg/pr.length).toFixed(1));
    $('#mdn').text(median(rssis));
    $('#mac_probes').html(html);
    // rssi chart
    drawRSSIChart(pr, d.pointBorderColor);
    // add link behavior and display more data
    $('.link-data').off('click');
    $('.link-data').on('click', function(e) {
      e.preventDefault();
      var range = $(this).data('range'), [start, end] = range.split('-');
      var html = '', indx;
      for (let i=0; i<_ds.length; i++) {
        if (_ds[i].label == _mac) {
          indx = i;
          break;
        }
      }
      for (let i=parseInt(start); i<=parseInt(end); i++) {
        var p = _ds[indx].data[i];
        html += '<tr><td class="ts">'+moment(p.x).format('YYYY-MM-DD HH:mm:ss')+'</td><td class="rssi">'+p.rssi+'</td><td>'+(d.ssids[p.ssid]||'&lt;empty&gt;')+'</td></tr>';
      }
      $(this).parents('tr').first().replaceWith(html);
    });
  }

  function drawMainChart(ctx, ds) {
    _chart = new Chart(ctx, {
      type: 'line',
      backgroundColor: '#ffffff',
      data: { datasets: ds, },
      options: {
        tooltips: {displayColors: false, titleFontFamily: 'monospace', intersect: false,
          callbacks: {
            title: function(tooltip, data) {
              var dsi = Array.isArray(tooltip) ? tooltip[0].datasetIndex : tooltip.datasetIndex;
              var t = data.datasets[dsi].label;
              var v = data.datasets[dsi].vendor;
              return t+'\n'+v;
            },
            label: function(tooltip, data) {
              var t = Array.isArray(tooltip) ? tooltip[0] : tooltip;
              var d = data.datasets[t.datasetIndex];
              var l = moment(d.data[t.index].x).format('YYYY-MM-DD HH:mm:ss');
              var r = d.data[t.index].rssi;
              var s = d.ssids[d.data[t.index].ssid] || '<empty>';
              return l+' / '+r+' / '+s;
            },
          }
        },
        responsive: true,
        animation: { duration: 0 },
        legend: {display: true, position: 'left',
          labels: {boxWidth: 25, fontSize: 12, fontFamily: 'monospace', fontColor: '#000',},
          onClick: function(e, item) {
            updateRSSIModal(_ds[item.datasetIndex]);
          },
          onHover: function(e, item) {
            $('html,body').css('cursor','pointer');
          },
          onLeave: function(e, item) {
            $('html,body').css('cursor','default');
          }
        },
        scales: {
          xAxes: [{
            ticks: {fontColor: '#000', padding: 5, fontSize: 12},
            gridLines: { zeroLineColor: '#000', color: '#ccc', tickMarkLength: 5},
            type: 'time',
            time: {
              unit: 'hour',
              stepSize: 1,
              displayFormats: { hour: 'HH[h]', }
            }
          }, {
            position: 'top',
            ticks: { display: false },
            gridLines: {zeroLineColor: '#000'}
          }],
          yAxes: [{
            ticks: { display: false, min: -1, max: ds.length },
            gridLines: { display: false, drawTicks: false, zeroLineColor: '#000' }
          }, {
            position: 'right',
            ticks: { display: false },
            gridLines: { display: false, drawTicks: false, zeroLineColor: '#000' }
          }],
        }
      }
    });
  }

  function updateTable(ds) {
    var macs = '', c = 0, color;
    for (let i=0; i<ds.length; i++) {
      var color = ds[i].pointBorderColor;
      var min = 100, max= -100, avg = 0, rssis = [];
      var prl = ds[i].data;
      for (let p of prl) {
        min = Math.min(min, p.rssi);
        max = Math.max(max, p.rssi);
        avg += p.rssi;
        rssis.push(p.rssi);
      }
      var ssids = ds[i].ssids.slice();
      if (ssids.indexOf('') > -1) {
        ssids.splice(ssids.indexOf(''), 1);
      }
      macs += '<tr class="small-mono mac-stats">';
      macs += '<td style="border-bottom: solid 1px '+color+';background-color:'+color+'"></td>';
      macs += '<td style="border-bottom: solid 1px '+color+'">'+ds[i].label+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'">'+prl.length+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'">'+min+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'">'+max+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'">'+(avg/prl.length).toFixed(1)+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'">'+median(rssis)+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'" class="ts">'+moment(prl[0].x).format('HH:mm:ss')+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'" class="ts">'+moment(prl[prl.length-1].x).format('HH:mm:ss')+'</td>';
      macs += '<td style="border-bottom: solid 1px '+color+'">'+(ssids.join(', ')||'&lt;none&gt;')+'</td></tr>';
    }
    $('#macs').html(macs);
    // but use the same modal
    $('.mac-stats').click(function(e) {
      e.preventDefault();
      $('#main-modal').show();
      var mac = $(this).children('td').eq(1).text();
      var indx = 0;
      for (let d of ds) {
        if (d.label == mac) {
          break;
        }
        indx += 1;
      }
      updateRSSIModal(ds[indx]);
    });
  }

  function refresh(chart, date, after, before) {
    // don't do anything if the modal is opened
    if (($("#main-modal").data('bs.modal') || {})._isShown) {
      return false;
    }
    updateDBTime();
    $('#loading').show();
    _mac = null; // to force update of modal data
    $('#msg').finish().addClass('alert-info').removeClass('alert-danger').text('Downloading data ...').show();
    // disable temporarly the datepicker
    $('#dp').datepicker('setStartDate', date).datepicker('setEndDate', date).datepicker('update', date);
    // disable refresh button
    $('#refresh').attr('disabled', 'disabled');

    var url, is_refreshed = false;
    if (typeof after == 'undefined' || typeof before == 'undefined') {
      var bounds = ts_bounds();
      if (bounds == null || !_can_refresh) {
        url = '/api/probes?today=true';
        _can_refresh = true;
      } else {
        is_refreshed = true;
        var before = moment().format('YYYY-MM-DDTHH:mm:ss');
        var after = moment(bounds.last_seen).format('YYYY-MM-DDTHH:mm:ss');
        url = '/api/probes?after='+after+'&before='+before;
      }
      _firstseen = moment().subtract(1, 'days').valueOf();
      _lastseen = moment().valueOf();
    } else {
      _can_refresh = false;
      url = '/api/probes?after='+after+'&before='+before;
      _firstseen = moment(after).valueOf()
      _lastseen = moment(before).valueOf();
    }
    // using protobuf
    fetch(url+'&output=protobuf').then(function(resp) {
      if (resp.ok) {
        resp.arrayBuffer().then(function(arrayBuffer) {
          var byteArray = new Uint8Array(arrayBuffer);
          var raw_data = proto.probemon.MyData.deserializeBinary(byteArray);
          var data = raw_data.getProbesList();

          if (data.length == 0) {
            $('#msg').removeClass('alert-info').addClass('alert-danger').text('No data found');
            $('#msg').fadeOut(5000, function() {$('#msg').removeClass('alert-danger').addClass('alert-info');});
            $('#loading').hide();
            if (chart !== null && !is_refreshed) {
              chart.destroy();
              _ds = [];
            }
            return false;
          }
          $('#loading').hide();
          $('#msg').text('Repacking data...');
          if (!is_refreshed) _ds = [];
          if (chart !== null) chart.destroy();
          update_data(data, _firstseen);
          if ($('#main-chart').is(':visible')) {
            // on desktop
            $('#msg').text('Drawing data...');
            drawMainChart(ctx, _ds);
          } else {
            $('#msg').text('Analyzing data...');
            // don't show the chart on mobile, but use a table
            updateTable(_ds);
          }
          $('#msg').hide();
        });
      }
    }).catch(function(error){
      $('#loading').hide();
      $('#msg').removeClass('alert-info').addClass('alert-danger').text('An error occured when downloading data');
      $('#msg').fadeOut(5000);
    }).finally(function() {
      // restore datepicker back to original state
      $('#dp').datepicker('setStartDate', false).datepicker('setEndDate', today).datepicker('update', date);
      $('#refresh').removeAttr('disabled');
    });
  }

  // draw initial message on canvas
  var ctx = document.getElementById('main-chart').getContext('2d');
  ctx.width = $('#main-chart').width();
  ctx.height = $('#main-chart').height();
  var dx = ctx.width/2;
  var dy = ctx.height/2;
  ctx.font = '30px sans-serif';
  ctx.fillStyle = 'grey';
  ctx.textAlign = 'center';
  ctx.fillText('Please wait...', dx, dy-30);
  ctx.fillText('while chart.js is rendering your chart.', dx, dy);

  // datepicker code
  var today = moment().format('YYYY-MM-DD');
  var dp_options = {container: '#dp', weekStart: 1, format: 'yyyy-mm-dd', endDate: today};
  $('#dp').datepicker(dp_options).on('changeDate', function (e) {
    // load data for the picked date
    var date = moment(e.date).format('YYYY-MM-DD');
    var after = date+'T00:00:00';
    var before = date+'T23:59:59';
    refresh(_chart, date, after, before);
    //
  });
  $('#dp').datepicker('update', today);

  $('#refresh').click(function() {
    var now = moment().format('YYYY-MM-DD');
    if (now > today) {
      today = now;
    }
    refresh(_chart, today);
  });

  $('#refresh').trigger('click');

  $('#complete-stats').click(function() {
    $('.carousel').carousel('next');
    // reset the panel
    $('#first-seen, #last-seen, #o-ssids, #o-count, #o-min, #o-max, #o-avg, #o-dop').text('?');
    $('#day-stats').html('<tr><td>Requesting server ...</td></tr>');
    $('#o-loading').show();
    $.ajax({
      url: '/api/stats/days?macs='+$('#mac').text(),
      dataType: 'json',
    }).done(function(data) {
      if (data.length == 0 || data[0].days.length == 0) {
        $('#day-stats').html('<tr><td>No data found</td></tr>');
        $('#o-loading').hide();
        return;
      }
      $('#first-seen').text(moment(data[0].days[0].first).format('YYYY-MM-DD HH:mm:ss'));
      $('#last-seen').text(moment(data[0].days[data[0].days.length-1].last).format('YYYY-MM-DD HH:mm:ss'));
      $('#o-ssids').text(data[0].ssids.join(', ') || '<none>');
      $('#o-dop').text(data[0].days.length);
      var html = '', count=0, min=100, max=-100, avg, med, sum=0;
      for (let d of data[0].days) {
        var day = d.day.substring(0,4)+'-'+d.day.substring(4,6)+'-'+d.day.substring(6,8);
        html += '<tr><td class="day">'+day+'</td><td>'+moment(d.first).format('HH:mm:ss')+'</td><td>'+moment(d.last).format('HH:mm:ss')+'</td><td>'+d.count+'</td><td>'+d.min+'</td><td>';
        html += d.max+'</td><td>'+d.avg+'</td><td>'+d.median+'</td></tr>';
        count += d.count;
        min = Math.min(min, d.min);
        if (d.max != 0) max = Math.max(max, d.max);
        sum += d.avg*d.count;
      }
      $('#o-count').text(count);
      $('#o-min').text(min);
      $('#o-max').text(max);
      $('#o-avg').text(Math.round(sum/count));
      $('#day-stats').html(html);
      $('#o-loading').hide();
    }).fail(function(jq, status, error){
        $('#day-stats').html('<tr><td>An error occured</td></tr>');
        $('#o-loading').hide();
    });
  });
  $('#rssi-stats').click(function() {
    $('.carousel').carousel('prev');
  });
  $('#main-modal').on('hidden.bs.modal', function (e) {
    $('.carousel').carousel(0);
  });

  $('#rawlogs').click(function() {
    // Generate link to collapse pre block
    var html = '';
    for (let i=0; i<24; i++) {
      var start = String(i).padStart(2, '0')+':00';
      var end = String(i+1).padStart(2, '0')+':00';
      if (end == '24:00') end = '00:00';
      html += `<a href="#rawlog-`+i+`-collapse" id="rawlog-`+i+`-link" data-toggle="collapse">Hour `+start+` to `+end+`</a>
              <div class="collapse" id="rawlog-`+i+`-collapse">
                <table class="table table-sm" id="rawlog-`+i+`-table" class="rawlog-table">
                  <tr><td>Loading...</td></tr>
                </table>
              </div><br>\n`;
    }
    $('#rawlogs-body').empty().append(html);
    var chosen_date = moment($('#dp').datepicker('getDate')).format('YYYY-MM-DD');
    $('#rawlogs-day').text(chosen_date);
    $('#rawlog-modal').modal('show');

    $('[id^="rawlog-"][id$="-link"]').on('click', function(e) {
      var chosen_date = moment($('#dp').datepicker('getDate')).format('YYYY-MM-DD');
      var hour = e.target.id.replace('rawlog-', '').replace('-link', '');
      fetch('/api/logs/raw?day='+chosen_date+'&hour='+hour).then(function(resp) {
        if (resp.ok) {
          return resp.json();
        } else {
          $('#rawlog-'+hour+'-table').empty().html('<tr><td>An error happened when loading data</td></tr>');
        }
      }).then(function(data) {
        var html = '';
        if (data['logs'].length != 0) {
          for (let i=0; i<data['logs'].length; i++) {
            var ts = moment.unix(data['logs'][i][0]).format('HH:mm:ss');
            var elmt = data['logs'][i];
            var mac = data['macs'][elmt[1]][0];
            var is_laa = Boolean(parseInt(mac.substring(0, 2), 16) & 0x2);
            html += '<tr><td>'+ts+'</td><td '+(is_laa? 'class="laa" ':'')+'title="'+data['macs'][elmt[1]][1]+'">'+mac;
            html += '</td><td>'+data['ssids'][elmt[2]]+'</td><td>'+elmt[3]+'</td></tr>';
          }
        } else {
          html = '<tr><td>No probe requests logged</td></tr>';
        }
        $('#rawlog-'+hour+'-table').empty().html(html);
      }).catch(function(error) {
        $('#rawlog-'+hour+'-table').empty().html('<tr><td>An error happened when loading data</td></tr>');
      });
    });

  });

  // refresh chart when window is visible again
  $(document).on('visibilitychange', function() {
    if (document.visibilityState == "visible") {
      var chosen_date = moment($('#dp').datepicker('getDate')).format('YYYY-MM-DD');
      if (chosen_date == today) {
        $('#refresh').trigger('click');
      }
    }
  });

  // code to sort table on header click (from https://stackoverflow.com/a/49041392/283067)
  const getCellValue = (tr, idx) => tr.children[idx].innerText || tr.children[idx].textContent;

  const comparer = (idx, asc) => (a, b) => ((v1, v2) =>
    v1 !== '' && v2 !== '' && !isNaN(v1) && !isNaN(v2) ? v1 - v2 : v1.toString().localeCompare(v2)
    )(getCellValue(asc ? a : b, idx), getCellValue(asc ? b : a, idx));

  document.querySelectorAll('th.sortable').forEach(th => th.addEventListener('click', (() => {
    const tbody = document.querySelector('#macs');
    Array.from(tbody.querySelectorAll('tr:nth-child(n)'))
      .sort(comparer(Array.from(th.parentNode.children).indexOf(th), this.asc = !this.asc))
      .forEach(tr => tbody.appendChild(tr) );
  })));
})
