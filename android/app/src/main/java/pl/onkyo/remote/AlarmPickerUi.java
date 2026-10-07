package pl.onkyo.remote;

import android.app.Dialog;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.view.Gravity;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

final class AlarmPickerUi {
    static final int INK=Color.rgb(238,234,226), NOTE=Color.rgb(187,181,170), ACCENT=Color.rgb(140,174,134);
    static int dp(Context context,int value){return Math.round(value*context.getResources().getDisplayMetrics().density);}
    static LinearLayout column(Context context){LinearLayout view=new LinearLayout(context);view.setOrientation(LinearLayout.VERTICAL);return view;}
    static TextView text(Context context,String value,int size){TextView view=new TextView(context);view.setText(value);view.setTextSize(size);view.setTextColor(INK);return view;}
    static GradientDrawable face(Context context,int fill,int stroke){
        GradientDrawable face=new GradientDrawable();face.setColor(fill);face.setCornerRadius(dp(context,8));face.setStroke(dp(context,1),stroke);return face;
    }
    static void style(Button view,int fill,int stroke){
        Context context=view.getContext();StateListDrawable states=new StateListDrawable();
        states.addState(new int[]{-android.R.attr.state_enabled},face(context,Color.rgb(36,36,36),Color.rgb(60,60,60)));
        states.addState(new int[]{android.R.attr.state_pressed},face(context,Color.rgb(58,65,54),ACCENT));
        states.addState(new int[]{android.R.attr.state_focused},face(context,fill,Color.rgb(233,215,171)));
        states.addState(new int[]{},face(context,fill,stroke));
        view.setBackground(states);view.setBackgroundTintList(null);view.setTextColor(new ColorStateList(new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},new int[]{Color.rgb(120,117,110),INK}));view.setAllCaps(false);
        view.setMinWidth(0);view.setMinimumWidth(0);view.setMinHeight(0);view.setMinimumHeight(0);
        view.setPadding(dp(context,12),dp(context,6),dp(context,12),dp(context,6));
    }
    static Button button(Context context,String value){Button view=new Button(context);view.setText(value);style(view,Color.rgb(41,41,41),Color.rgb(85,85,85));return view;}
    static Dialog dialog(Context context,LinearLayout body){
        Dialog dialog=new Dialog(context);dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        ScrollView scroll=new ScrollView(context);scroll.setBackground(face(context,Color.rgb(32,32,32),Color.rgb(101,101,93)));
        body.setPadding(dp(context,20),dp(context,20),dp(context,20),dp(context,20));scroll.addView(body);dialog.setContentView(scroll);
        Window window=dialog.getWindow();if(window!=null){window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));window.setWindowAnimations(0);}
        dialog.setCanceledOnTouchOutside(true);return dialog;
    }
    static void show(Dialog dialog){
        dialog.show();Window window=dialog.getWindow();if(window==null)return;
        window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        Context context=dialog.getContext();int width=Math.min(dp(context,340),context.getResources().getDisplayMetrics().widthPixels-dp(context,32));
        window.setLayout(width,WindowManager.LayoutParams.WRAP_CONTENT);
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE|WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
    }
    static LinearLayout row(Context context){LinearLayout view=new LinearLayout(context);view.setOrientation(LinearLayout.HORIZONTAL);view.setGravity(Gravity.CENTER_VERTICAL);return view;}
}
