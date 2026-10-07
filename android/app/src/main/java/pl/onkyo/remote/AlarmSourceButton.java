package pl.onkyo.remote;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.widget.Button;
import android.widget.LinearLayout;
import java.util.Arrays;

public final class AlarmSourceButton extends Button {
    private String value="TUNER";
    private Dialog editor;
    public AlarmSourceButton(Context context){
        super(context);AlarmPickerUi.style(this,Color.rgb(41,41,41),Color.rgb(85,85,85));setTextSize(15);
        setCompoundDrawablesRelativeWithIntrinsicBounds(0,0,R.drawable.ic_chevron_down,0);setValue(value);setOnClickListener(v->openEditor());
    }
    public String getValue(){return value;}
    public void setValue(String source){if(!Arrays.asList(AlarmSnapshot.SOURCES).contains(source))throw new IllegalArgumentException("Invalid source");value=source;setText(value);setContentDescription("Źródło pobudki: "+value);}
    @Override public void setEnabled(boolean enabled){super.setEnabled(enabled);setAlpha(enabled?1f:.45f);if(!enabled)dismissEditor();}
    void dismissEditor(){if(editor!=null)editor.dismiss();}
    private void openEditor(){
        if(!isEnabled())return;LinearLayout body=AlarmPickerUi.column(getContext());body.addView(AlarmPickerUi.text(getContext(),"Źródło pobudki",20));
        for(String source:AlarmSnapshot.SOURCES){Button option=AlarmPickerUi.button(getContext(),source);
            if(value.equals(source))AlarmPickerUi.style(option,Color.rgb(70,93,64),AlarmPickerUi.ACCENT);
            option.setContentDescription("Wybierz źródło "+source);option.setOnClickListener(v->{setValue(source);dismissEditor();});
            LinearLayout.LayoutParams layout=new LinearLayout.LayoutParams(-1,AlarmPickerUi.dp(getContext(),48));layout.topMargin=AlarmPickerUi.dp(getContext(),8);body.addView(option,layout);
        }
        editor=AlarmPickerUi.dialog(getContext(),body);editor.setOnDismissListener(dialog->{editor=null;requestFocus();});AlarmPickerUi.show(editor);
    }
}
