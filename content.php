<div id="global" class="settings">
<fieldset>
<legend>Sync MultiSync Configuration</legend>

Enable Sync Plugin:  <? PrintSettingCheckbox("Enable Sync", "SyncEnable", 1, 0, "1", "0", "fpp-Sync"); ?>
<p>

The FPP Sync plugin supports using the UART based Sync modules to send/receive MultiSync packets via Sync wireless.   It currently
supports the EBYTE "E32-*" modules including the E32-915T30D, E32-433T20D, etc...  They can be wired directly to the UART pins on
the Pi/BBB or by pluging into the USB port using a E15-USB-T2 adapter (recommended).   Make sure the M1 and M2 pins are pulled
low (M1 and M2) jumpers are in place on the E15-USB-T2).
<p>
If FPP is in Master mode, it will send MultiSync packets out.   If in Remote mode, it will listen for packets.
<p>
These setting much match what the module has been configured to use.<br>
<p>
The Sync protocol is very slow, it defaults to 2400 baud over the air.   At the start of each sequence, we have to send the
filenames for the sequence and the media which can take time.  If you know the remotes will not need the media filenames, turn
off sending the media sync packets which will help the remotes start quicker.
<p>
Bridging to the local network will re-send the sync packets out on the local network so other FPP devices on can
be synced.   This remote must have it's MultiSync targets set while in Master mode once before being placed back
into Remote mode.

</fieldset>
<br>

<fieldset>
<legend>Sync Device Configuration</legend>
In order to configure the Sync device, FPPD must be running with the Sync plugin loaded.
The module must be put into Sleep mode by pulling M1 and M2 high.  If using the E15-USB-T2 adapter, just remove
the M1 and M2 jumpers.   On submit, the settings below will be saved onto the device.
<p>

<form  id="SyncconfigForm" action="api/plugin-apis/Sync" method="post">
Module Address (0-65535): <input name="MA" type="number" required min="0" max="65535" value="0">
<br>
UART Baud Rate:
<select name="UBR">
<option value="1200">1200</option>
<option value="2400">2400</option>
<option value="4800">4800</option>
<option value="9600" selected>9600</option>
<option value="19200">19200</option>
<option value="38400">38400</option>
<option value="57600">57600</option>
<option value="115200">115200</option>
</select>
<br>
Air Data Rate: <select name="ADR">
<option value="300">300</option>
<option value="1200">1200</option>
<option value="2400" selected>2400</option>
<option value="4800">4800</option>
<option value="9600">9600</option>
<option value="19200">19200</option>
</select>
<br>
FEC (Forward Error Correction): <input name="FEC" type="checkbox" value="1" checked/>
<br>
Transmit Power: <select name="TXP">
<option value="4" selected>High</option>
<option value="3">Medium</option>
<option value="2">Low</option>
<option value="1">Very Low</option>
</select>
<br>
Channel/Frequency (0-255): <input name="CH" type="number" required min="0" max="255" value="23">
<br>
The Channel/Frequency setting depends on the Module.  For the 433Mhz modules, the frequency will be 410 plus
the setting.   Thus, a setting of 23 will result in the default frequency of 433Mhz.    For the 868Mhz modules,
it is 862 + the setting so the default is 6.   For the 915Mhz, it is 900 plus the setting, so the default is 15.
<br>
<input type="submit" value="Save">

</form>


<script type='text/javascript'>
$("#SyncconfigForm").submit(function(event) {
     event.preventDefault();
     var $form = $( this ),
     url = $form.attr( 'action' );
     
     /* Send the data using post with element id name and name2*/
     var posting = $.post( url, $form.serialize());
     
     /* Alerts the results */
     posting.done(function( data ) {
                  //alert('success');
                  });
     });
</script>
</fieldset>
