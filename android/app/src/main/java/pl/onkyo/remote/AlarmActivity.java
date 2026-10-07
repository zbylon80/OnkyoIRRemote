package pl.onkyo.remote;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.os.Bundle;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import java.util.Locale;

/** The ESP32 owns the clock and executes alarms; this screen only reads/writes its settings. */
public final class AlarmActivity extends Activity {
    private String endpoint;
    private int widgetId;
    private boolean busy, loaded, clockReady, storageReady;
    private Spinner onHour, onMinute, offHour, offMinute, source;
    private Button onSet, offSet, onCancel, offCancel, refresh, back;
    private TextView clock, result, onStatus, offStatus;
    private Bundle restoredDraft;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        widgetId = getIntent().getIntExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, AppWidgetManager.INVALID_APPWIDGET_ID);
        String configured = WidgetSettings.endpoint(this, widgetId);
        if (configured.isEmpty()) { finish(); return; }
        try { endpoint = RemoteEndpoint.normalize(configured); }
        catch (IllegalArgumentException e) { finish(); return; }
        restoredDraft = state;
        buildScreen();
        request(null);
    }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private LinearLayout column() { LinearLayout view = new LinearLayout(this); view.setOrientation(LinearLayout.VERTICAL); return view; }
    private LinearLayout row() { LinearLayout view = new LinearLayout(this); view.setOrientation(LinearLayout.HORIZONTAL); view.setGravity(Gravity.CENTER_VERTICAL); return view; }
    private TextView text(String value, int size) {
        TextView view = new TextView(this); view.setText(value); view.setTextSize(size); view.setTextColor(Color.rgb(238,234,226)); return view;
    }
    private Button button(String value) {
        Button view = new Button(this); view.setText(value); view.setAllCaps(false); view.setMinHeight(dp(48)); return view;
    }
    private Spinner spinner(String[] values, String description) {
        Spinner view = new Spinner(this, Spinner.MODE_DROPDOWN);
        // Compact selectors need room for the two digits as well as the native arrow.
        view.setPadding(dp(6),0,dp(28),0);
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, values) {
            @Override public View getView(int position, View recycled, android.view.ViewGroup parent) {
                TextView selected=(TextView)super.getView(position,recycled,parent);
                selected.setPadding(0,0,0,0); selected.setTextColor(Color.rgb(238,234,226));
                selected.setTextSize(16); selected.setGravity(Gravity.CENTER_VERTICAL);
                return selected;
            }
        };
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        view.setAdapter(adapter); view.setContentDescription(description);
        return view;
    }
    private String[] numbers(int count) {
        String[] values = new String[count];
        for (int i=0;i<count;i++) values[i]=String.format(Locale.ROOT,"%02d",i);
        return values;
    }
    private void timeRow(LinearLayout section, Spinner hour, Spinner minute) {
        LinearLayout row = row();
        TextView label=text("Godzina",14);label.setGravity(Gravity.CENTER_VERTICAL);
        row.addView(label,new LinearLayout.LayoutParams(0,dp(48),1));
        row.addView(hour,new LinearLayout.LayoutParams(dp(80),dp(48)));
        row.addView(text(":",18));
        row.addView(minute,new LinearLayout.LayoutParams(dp(80),dp(48)));
        section.addView(row);
    }
    private LinearLayout section(String title, int accent) {
        LinearLayout view = column(); view.setPadding(dp(14),dp(12),dp(14),dp(12));
        GradientDrawable background = new GradientDrawable(); background.setColor(Color.rgb(32,32,32));
        background.setCornerRadius(dp(8)); background.setStroke(dp(1),accent); view.setBackground(background);
        LinearLayout.LayoutParams layout = new LinearLayout.LayoutParams(-1,-2); layout.topMargin=dp(16); view.setLayoutParams(layout);
        TextView heading = text(title,19); heading.setTypeface(null,Typeface.BOLD); view.addView(heading); return view;
    }
    private void buildScreen() {
        ScrollView root = new ScrollView(this); root.setFillViewport(true); setContentView(root);
        root.setOnApplyWindowInsetsListener((view,insets)->{
            view.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom()); return insets;
        });
        LinearLayout body = column(); body.setPadding(dp(18),dp(18),dp(18),dp(24)); root.addView(body);
        LinearLayout header = row(); TextView title = text("Budzik",24); title.setTypeface(null,Typeface.BOLD);
        header.addView(title,new LinearLayout.LayoutParams(0,-2,1)); back=button("← Widżet"); back.setOnClickListener(v->finish()); header.addView(back); body.addView(header);
        clock=text("Odczytywanie czasu ESP32…",12); body.addView(clock);
        onHour=spinner(numbers(24),"Godzina włączenia"); onMinute=spinner(numbers(60),"Minuty włączenia");
        offHour=spinner(numbers(24),"Godzina wyłączenia"); offMinute=spinner(numbers(60),"Minuty wyłączenia");
        source=spinner(AlarmSnapshot.SOURCES,"Źródło pobudki");
        LinearLayout on=section("☀ Włącz",Color.rgb(140,174,134)); body.addView(on);
        timeRow(on,onHour,onMinute);
        LinearLayout sources=row(); sources.addView(text("Źródło",14),new LinearLayout.LayoutParams(0,-2,1)); sources.addView(source,new LinearLayout.LayoutParams(dp(148),dp(48))); on.addView(sources);
        LinearLayout onActions=row(); onSet=button("Ustaw"); onSet.setBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.rgb(70,93,64)));
        onSet.setContentDescription("Ustaw włączenie");
        onSet.setOnClickListener(v->request("on")); onActions.addView(onSet,new LinearLayout.LayoutParams(0,-2,1));
        onCancel=button("×"); onCancel.setContentDescription("Anuluj włączenie"); onCancel.setOnClickListener(v->request("cancelOn")); onCancel.setVisibility(View.GONE); onActions.addView(onCancel,new LinearLayout.LayoutParams(dp(48),-2)); on.addView(onActions);
        onStatus=text("",12); on.addView(onStatus);
        LinearLayout off=section("☾ Wyłącz",Color.rgb(158,133,130)); body.addView(off); timeRow(off,offHour,offMinute);
        LinearLayout offActions=row(); offSet=button("Ustaw"); offSet.setBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.rgb(85,67,64)));
        offSet.setContentDescription("Ustaw wyłączenie");
        offSet.setOnClickListener(v->request("off")); offActions.addView(offSet,new LinearLayout.LayoutParams(0,-2,1));
        offCancel=button("×"); offCancel.setContentDescription("Anuluj wyłączenie"); offCancel.setOnClickListener(v->request("cancelOff")); offCancel.setVisibility(View.GONE); offActions.addView(offCancel,new LinearLayout.LayoutParams(dp(48),-2)); off.addView(offActions);
        offStatus=text("",12); off.addView(offStatus);
        result=text("",13); result.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE); body.addView(result);
        LinearLayout footer=row(); footer.setPadding(0,dp(16),0,0); footer.addView(text("Każda akcja wykona się raz.",12),new LinearLayout.LayoutParams(0,-2,1));
        refresh=button("Odśwież"); refresh.setOnClickListener(v->request(null)); footer.addView(refresh); body.addView(footer);
        onHour.setSelection(7); offHour.setSelection(2);
        restoreDraft(); updateEnabled();
    }

    private String time(boolean on) {
        return (on?onHour:offHour).getSelectedItem()+":"+(on?onMinute:offMinute).getSelectedItem();
    }
    private void updateEnabled() {
        for (Spinner field:new Spinner[]{onHour,onMinute,offHour,offMinute,source}) field.setEnabled(!busy&&loaded&&storageReady);
        onSet.setEnabled(!busy&&loaded&&storageReady&&clockReady); offSet.setEnabled(onSet.isEnabled());
        onCancel.setEnabled(!busy&&loaded&&storageReady); offCancel.setEnabled(onCancel.isEnabled());
        refresh.setEnabled(!busy); back.setEnabled(!busy);
    }
    private void display(AlarmSnapshot data, String action) {
        loaded=true; storageReady=data.storageReady; clockReady=data.clockReady;
        clock.setText(clockReady?"Czas ESP32: "+(data.localTime.length()>=19?data.localTime.substring(11):data.localTime)+" · Polska":"Zegar ESP32 czeka na synchronizację.");
        if (action==null||action.equals("on")||action.equals("cancelOn")) {
            onHour.setSelection(Integer.parseInt(data.on.time.substring(0,2))); onMinute.setSelection(Integer.parseInt(data.on.time.substring(3)));
            source.setSelection(java.util.Arrays.asList(AlarmSnapshot.SOURCES).indexOf(data.source));
        }
        if (action==null||action.equals("off")||action.equals("cancelOff")) {
            offHour.setSelection(Integer.parseInt(data.off.time.substring(0,2))); offMinute.setSelection(Integer.parseInt(data.off.time.substring(3)));
        }
        onStatus.setText(data.on.status()); offStatus.setText(data.off.status());
        onCancel.setVisibility(data.on.enabled?View.VISIBLE:View.GONE); offCancel.setVisibility(data.off.enabled?View.VISIBLE:View.GONE);
        if(action==null){restoreDraft();restoredDraft=null;}
    }
    private void request(String action) {
        if(busy||endpoint==null)return;
        if(action!=null&&(!loaded||!storageReady||(!action.startsWith("cancel")&&!clockReady)))return;
        if(!DeviceGate.acquire(endpoint)){result.setText("Trwa inne polecenie. Odśwież za chwilę.");return;}
        final String selectedTime=time("on".equals(action)),selectedSource=(String)source.getSelectedItem();
        busy=true;updateEnabled();result.setText(action==null?"Odczytywanie…":"Zapisywanie…");
        final String oldStatus=WidgetSettings.status(this,widgetId);
        new Thread(()->{
            AlarmSnapshot snapshot=null;String error=null;
            try{
                OnkyoWidgetProvider.updateDevice(this,endpoint,"Ustawienia budzika…",true);
                snapshot=new AlarmSnapshot(RemoteClient.alarms(endpoint,action,selectedTime,selectedSource));
            }catch(Exception e){error=e.getMessage();}
            finally{
                try{OnkyoWidgetProvider.updateDevice(this,endpoint,oldStatus,false);}
                finally{DeviceGate.release(endpoint);}
            }
            final AlarmSnapshot response=snapshot;final String failure=error;
            runOnUiThread(()->{
                if(isFinishing()||isDestroyed())return;
                busy=false;
                if(response==null){loaded=false;result.setText((action==null?"Nie można odczytać budzika. ":"Brak potwierdzenia zapisu. ")+(failure==null?"":failure)+" Odśwież ustawienia.");}
                else{
                    display(response,action);
                    result.setText(!storageReady?"Pamięć ESP32 niedostępna. Budzik zatrzymany.":action==null?"":action.startsWith("cancel")?"Anulowano.":"Ustawione. Możesz wrócić do widżetu.");
                }
                updateEnabled();
            });
        },"onkyo-alarms").start();
    }
    private void restoreDraft(){
        if(restoredDraft==null)return;
        Spinner[] fields={onHour,onMinute,offHour,offMinute,source};
        for(int i=0;i<fields.length;i++)fields[i].setSelection(Math.min(fields[i].getCount()-1,Math.max(0,restoredDraft.getInt("draft"+i,fields[i].getSelectedItemPosition()))));
    }
    @Override protected void onSaveInstanceState(Bundle state){
        super.onSaveInstanceState(state);
        if(onHour==null)return;
        Spinner[] fields={onHour,onMinute,offHour,offMinute,source};
        for(int i=0;i<fields.length;i++)state.putInt("draft"+i,fields[i].getSelectedItemPosition());
    }
}
