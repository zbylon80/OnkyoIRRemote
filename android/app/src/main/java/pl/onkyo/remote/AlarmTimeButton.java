package pl.onkyo.remote;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.text.InputFilter;
import android.text.InputType;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.util.Locale;

/** Local time draft. Only AlarmActivity's Set action writes to the device. */
public final class AlarmTimeButton extends Button {
    private final String title;
    private String time="07:00";
    private Dialog editor;
    private EditText hour,minute;
    private TextView error;
    public AlarmTimeButton(Context context,String title){
        super(context);this.title=title;AlarmPickerUi.style(this,Color.rgb(41,41,41),Color.rgb(85,85,85));
        setTypeface(Typeface.MONOSPACE);setTextSize(28);setCompoundDrawablesRelativeWithIntrinsicBounds(0,0,R.drawable.ic_chevron_down,0);
        setCompoundDrawablePadding(AlarmPickerUi.dp(context,8));setTime(time);setOnClickListener(v->openEditor());
    }
    public String getTime(){return time;}
    public void setTime(String value){
        if(!value.matches("(?:[01][0-9]|2[0-3]):[0-5][0-9]"))throw new IllegalArgumentException("Expected HH:mm");
        time=value;setText(value);setContentDescription(title+": "+value);
    }
    @Override public void setEnabled(boolean enabled){super.setEnabled(enabled);setAlpha(enabled?1f:.45f);if(!enabled)dismissEditor();}
    public void dismissEditor(){if(editor!=null)editor.dismiss();}
    private int dp(int value){return AlarmPickerUi.dp(getContext(),value);}
    private static int number(EditText input,int count){
        String value=input.getText().toString();if(!value.matches("[0-9]{1,2}"))return -1;
        int number=Integer.parseInt(value);return number<count?number:-1;
    }
    private void step(EditText input,int count,int change){
        int value=number(input,count);if(value<0)value=Integer.parseInt(count==24?time.substring(0,2):time.substring(3));
        input.setText(String.format(Locale.ROOT,"%02d",(value+change+count)%count));error.setText("");
    }
    private EditText input(String description){
        EditText field=new EditText(getContext());field.setTextColor(AlarmPickerUi.INK);field.setTextSize(42);field.setTypeface(Typeface.MONOSPACE);
        field.setGravity(Gravity.CENTER);field.setSingleLine(true);field.setInputType(InputType.TYPE_CLASS_NUMBER);
        field.setFilters(new InputFilter[]{new InputFilter.LengthFilter(2)});field.setSelectAllOnFocus(true);
        field.setBackground(AlarmPickerUi.face(getContext(),Color.rgb(21,21,21),Color.rgb(85,85,85)));field.setBackgroundTintList(null);
        field.setContentDescription(description);field.setPadding(dp(4),0,dp(4),0);return field;
    }
    private LinearLayout part(String label,EditText field,int count,String unit){
        LinearLayout column=AlarmPickerUi.column(getContext());TextView caption=AlarmPickerUi.text(getContext(),label,11);
        caption.setTextColor(AlarmPickerUi.NOTE);caption.setGravity(Gravity.CENTER);column.addView(caption,new LinearLayout.LayoutParams(-1,dp(24)));
        Button up=AlarmPickerUi.button(getContext(),"▲");up.setContentDescription("Zmniejsz "+unit);up.setOnClickListener(v->step(field,count,-1));column.addView(up,new LinearLayout.LayoutParams(-1,dp(48)));
        LinearLayout.LayoutParams inputLayout=new LinearLayout.LayoutParams(-1,dp(72));inputLayout.topMargin=dp(6);inputLayout.bottomMargin=dp(6);column.addView(field,inputLayout);
        Button down=AlarmPickerUi.button(getContext(),"▼");down.setContentDescription("Zwiększ "+unit);down.setOnClickListener(v->step(field,count,1));column.addView(down,new LinearLayout.LayoutParams(-1,dp(48)));
        field.setOnKeyListener((v,key,event)->{
            if(event.getAction()!=KeyEvent.ACTION_DOWN)return false;
            if(key==KeyEvent.KEYCODE_DPAD_DOWN||key==KeyEvent.KEYCODE_DPAD_UP){step(field,count,key==KeyEvent.KEYCODE_DPAD_DOWN?1:-1);return true;}return false;
        });return column;
    }
    private void commit(){
        int h=number(hour,24),m=number(minute,60);
        if(h<0||m<0){error.setText("Wpisz godzinę 00–23 i minuty 00–59.");return;}
        setTime(String.format(Locale.ROOT,"%02d:%02d",h,m));dismissEditor();
    }
    private void openEditor(){
        if(!isEnabled()||(editor!=null&&editor.isShowing()))return;
        Context context=getContext();LinearLayout body=AlarmPickerUi.column(context);
        TextView heading=AlarmPickerUi.text(context,title,20);body.addView(heading);
        TextView note=AlarmPickerUi.text(context,"Format 24-godzinny",12);note.setTextColor(AlarmPickerUi.NOTE);body.addView(note);
        hour=input("Godzina, od 00 do 23");minute=input("Minuta, od 00 do 59");hour.setText(time.substring(0,2));minute.setText(time.substring(3));
        hour.setImeOptions(EditorInfo.IME_ACTION_NEXT);minute.setImeOptions(EditorInfo.IME_ACTION_DONE);
        minute.setOnEditorActionListener((view,action,event)->{if(action==EditorInfo.IME_ACTION_DONE){commit();return true;}return false;});
        LinearLayout digits=AlarmPickerUi.row(context);LinearLayout.LayoutParams digitsLayout=new LinearLayout.LayoutParams(-1,-2);digitsLayout.topMargin=dp(16);
        digits.addView(part("GODZINA",hour,24,"godzinę"),new LinearLayout.LayoutParams(0,-2,1));
        TextView colon=AlarmPickerUi.text(context,":",38);colon.setGravity(Gravity.CENTER);colon.setTextColor(AlarmPickerUi.NOTE);digits.addView(colon,new LinearLayout.LayoutParams(dp(32),-2));
        digits.addView(part("MINUTA",minute,60,"minutę"),new LinearLayout.LayoutParams(0,-2,1));body.addView(digits,digitsLayout);
        error=AlarmPickerUi.text(context,"",12);error.setTextColor(Color.rgb(231,170,161));error.setAccessibilityLiveRegion(ACCESSIBILITY_LIVE_REGION_POLITE);LinearLayout.LayoutParams errorLayout=new LinearLayout.LayoutParams(-1,-2);errorLayout.topMargin=dp(10);body.addView(error,errorLayout);
        LinearLayout actions=AlarmPickerUi.row(context);Button cancel=AlarmPickerUi.button(context,"Anuluj");cancel.setOnClickListener(v->dismissEditor());
        actions.addView(cancel,new LinearLayout.LayoutParams(0,dp(48),1));Button done=AlarmPickerUi.button(context,"Gotowe");AlarmPickerUi.style(done,Color.rgb(70,93,64),AlarmPickerUi.ACCENT);
        done.setOnClickListener(v->commit());LinearLayout.LayoutParams doneLayout=new LinearLayout.LayoutParams(0,dp(48),1);doneLayout.leftMargin=dp(10);actions.addView(done,doneLayout);
        LinearLayout.LayoutParams actionsLayout=new LinearLayout.LayoutParams(-1,-2);actionsLayout.topMargin=dp(12);body.addView(actions,actionsLayout);
        editor=AlarmPickerUi.dialog(context,body);editor.setOnDismissListener(dialog->{editor=null;requestFocus();});AlarmPickerUi.show(editor);
    }
    void saveDraft(Bundle state,String key){
        state.putString(key,time);if(editor!=null&&editor.isShowing()){state.putBoolean(key+"Open",true);state.putString(key+"Hour",hour.getText().toString());state.putString(key+"Minute",minute.getText().toString());}
    }
    void restoreEditor(Bundle state,String key){if(state.getBoolean(key+"Open"))post(()->{
        if(!isEnabled())return;openEditor();if(editor!=null){hour.setText(state.getString(key+"Hour",time.substring(0,2)));minute.setText(state.getString(key+"Minute",time.substring(3)));}
    });}
}
