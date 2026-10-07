package pl.onkyo.remote;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.os.Bundle;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/** The ESP32 owns the clock and executes alarms; this screen only reads/writes its settings. */
public final class AlarmActivity extends Activity {
    private String endpoint;
    private int widgetId;
    private boolean busy, loaded, clockReady, storageReady;
    private AlarmTimeButton onTime, offTime;
    private AlarmSourceButton source;
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
        Button view = AlarmPickerUi.button(this,value); view.setMinHeight(dp(48)); return view;
    }
    private void timeRow(LinearLayout section, AlarmTimeButton time) {
        LinearLayout row = row();
        TextView label=text("Godzina",14);label.setGravity(Gravity.CENTER_VERTICAL);
        row.addView(label,new LinearLayout.LayoutParams(0,dp(48),1));
        row.addView(time,new LinearLayout.LayoutParams(dp(156),dp(54)));
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
        onTime=new AlarmTimeButton(this,"Godzina włączenia");offTime=new AlarmTimeButton(this,"Godzina wyłączenia");offTime.setTime("02:00");
        source=new AlarmSourceButton(this);
        LinearLayout on=section("☀ Włącz",Color.rgb(140,174,134)); body.addView(on);
        timeRow(on,onTime);
        LinearLayout sources=row(); sources.setPadding(0,dp(6),0,dp(6));sources.addView(text("Źródło",14),new LinearLayout.LayoutParams(0,-2,1)); sources.addView(source,new LinearLayout.LayoutParams(dp(156),dp(48))); on.addView(sources);
        LinearLayout onActions=row(); onSet=button("Ustaw"); AlarmPickerUi.style(onSet,Color.rgb(70,93,64),AlarmPickerUi.ACCENT);
        onSet.setContentDescription("Ustaw włączenie");
        onSet.setOnClickListener(v->request("on")); onActions.addView(onSet,new LinearLayout.LayoutParams(0,-2,1));
        onCancel=button("×"); onCancel.setContentDescription("Anuluj włączenie"); onCancel.setOnClickListener(v->request("cancelOn")); onCancel.setVisibility(View.GONE); onActions.addView(onCancel,new LinearLayout.LayoutParams(dp(48),-2)); on.addView(onActions);
        onStatus=text("",12); on.addView(onStatus);
        LinearLayout off=section("☾ Wyłącz",Color.rgb(158,133,130)); body.addView(off); timeRow(off,offTime);
        LinearLayout offActions=row(); offActions.setPadding(0,dp(6),0,dp(6));offSet=button("Ustaw"); AlarmPickerUi.style(offSet,Color.rgb(85,67,64),Color.rgb(158,133,130));
        offSet.setContentDescription("Ustaw wyłączenie");
        offSet.setOnClickListener(v->request("off")); offActions.addView(offSet,new LinearLayout.LayoutParams(0,-2,1));
        offCancel=button("×"); offCancel.setContentDescription("Anuluj wyłączenie"); offCancel.setOnClickListener(v->request("cancelOff")); offCancel.setVisibility(View.GONE); offActions.addView(offCancel,new LinearLayout.LayoutParams(dp(48),-2)); off.addView(offActions);
        offStatus=text("",12); off.addView(offStatus);
        result=text("",13); result.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE); body.addView(result);
        LinearLayout footer=row(); footer.setPadding(0,dp(16),0,0); footer.addView(text("Każda akcja wykona się raz.",12),new LinearLayout.LayoutParams(0,-2,1));
        refresh=button("Odśwież"); refresh.setOnClickListener(v->request(null)); footer.addView(refresh); body.addView(footer);
        restoreDraft(); updateEnabled();
    }

    private String time(boolean on) {
        return (on?onTime:offTime).getTime();
    }
    private void updateEnabled() {
        onTime.setEnabled(!busy&&loaded&&storageReady);offTime.setEnabled(onTime.isEnabled());source.setEnabled(onTime.isEnabled());
        onSet.setEnabled(!busy&&loaded&&storageReady&&clockReady); offSet.setEnabled(onSet.isEnabled());
        onCancel.setEnabled(!busy&&loaded&&storageReady); offCancel.setEnabled(onCancel.isEnabled());
        refresh.setEnabled(!busy); back.setEnabled(!busy);
    }
    private void display(AlarmSnapshot data, String action) {
        loaded=true; storageReady=data.storageReady; clockReady=data.clockReady;
        clock.setText(clockReady?"Czas ESP32: "+(data.localTime.length()>=19?data.localTime.substring(11):data.localTime)+" · Polska":"Zegar ESP32 czeka na synchronizację.");
        if (action==null||action.equals("on")||action.equals("cancelOn")) {
            onTime.setTime(data.on.time);source.setValue(data.source);
        }
        if (action==null||action.equals("off")||action.equals("cancelOff")) {
            offTime.setTime(data.off.time);
        }
        onStatus.setText(data.on.status()); offStatus.setText(data.off.status());
        onCancel.setVisibility(data.on.enabled?View.VISIBLE:View.GONE); offCancel.setVisibility(data.off.enabled?View.VISIBLE:View.GONE);
        if(action==null){restoreDraft();if(restoredDraft!=null){onTime.restoreEditor(restoredDraft,"onTime");offTime.restoreEditor(restoredDraft,"offTime");}restoredDraft=null;}
    }
    private void request(String action) {
        if(busy||endpoint==null)return;
        if(action!=null&&(!loaded||!storageReady||(!action.startsWith("cancel")&&!clockReady)))return;
        if(!DeviceGate.acquire(endpoint)){result.setText("Trwa inne polecenie. Odśwież za chwilę.");return;}
        final String selectedTime=time("on".equals(action)),selectedSource=source.getValue();
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
        onTime.setTime(restoredDraft.getString("onTime",onTime.getTime()));offTime.setTime(restoredDraft.getString("offTime",offTime.getTime()));
        source.setValue(restoredDraft.getString("source",source.getValue()));
    }
    @Override protected void onSaveInstanceState(Bundle state){
        super.onSaveInstanceState(state);
        if(onTime==null)return;
        onTime.saveDraft(state,"onTime");offTime.saveDraft(state,"offTime");state.putString("source",source.getValue());
    }
    @Override protected void onDestroy(){if(onTime!=null){onTime.dismissEditor();offTime.dismissEditor();source.dismissEditor();}super.onDestroy();}
}
