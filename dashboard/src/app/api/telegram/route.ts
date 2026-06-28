import { NextRequest, NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

const BOT_TOKEN = process.env.TELEGRAM_BOT_TOKEN;

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    console.log('[Telegram Webhook] Received body:', JSON.stringify(body));

    // Handle Button Clicks (Callback Queries)
    if (body.callback_query) {
      const query = body.callback_query;
      const chatId = query.message.chat.id;
      const data = query.data; 
      
      let label = '24 Hours';
      let hours = 24;
      
      if (data === 'report_week') { label = '7 Days'; hours = 168; }
      else if (data === 'report_month') { label = '30 Days'; hours = 720; }
      else if (data === 'report_year') { label = '1 Year'; hours = 8760; }

      const since = new Date(Date.now() - hours * 60 * 60 * 1000).toISOString();
      const { data: readings, error } = await supabase
        .from('room_readings')
        .select('temperature,humidity')
        .gte('created_at', since)
        .order('created_at', { ascending: false });

      let responseText = '';
      if (error || !readings || readings.length === 0) {
        responseText = `⚠️ No data found for the last ${label}.`;
      } else {
        const avgT = readings.reduce((acc, r) => acc + r.temperature, 0) / readings.length;
        const avgH = readings.reduce((acc, r) => acc + r.humidity, 0) / readings.length;
        responseText = `📋 *AETHER REPORT: ${label.toUpperCase()}*\n\n🌡 *Avg Temp:* ${avgT.toFixed(1)}°C\n💧 *Avg Hum:* ${avgH.toFixed(1)}%\n📊 *Samples:* ${readings.length}\n\n_Generated from latest device syncs._`;
      }

      await fetch(`https://api.telegram.org/bot${BOT_TOKEN}/sendMessage`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ chat_id: chatId, text: responseText, parse_mode: 'Markdown' }),
      });

      // Answer callback to remove loading state on button
      await fetch(`https://api.telegram.org/bot${BOT_TOKEN}/answerCallbackQuery`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ callback_query_id: query.id }),
      });

      return NextResponse.json({ ok: true });
    }

    const message = body.message || body.edited_message;
    if (!message || !message.text) {
      console.log('[Telegram Webhook] No text found in message');
      return NextResponse.json({ ok: true });
    }

    const chatId = message.chat.id;
    const text = message.text.trim().toLowerCase();
    console.log(`[Telegram Webhook] Command: ${text} from Chat: ${chatId}`);

    if (!BOT_TOKEN) {
      console.error('[Telegram Webhook] CRITICAL: TELEGRAM_BOT_TOKEN is not set!');
      return NextResponse.json({ ok: false, error: 'Missing Token' }, { status: 500 });
    }

    let responseText = '';
    let replyMarkup = null;

    if (text.startsWith('/start')) {
      responseText = "👋 I'm Aether, your room monitor mascot! I've been successfully linked to your dashboard.\n\nTry /status to see live data.";
    } else if (text.startsWith('/url')) {
      const host = req.headers.get('host');
      const siteUrl = process.env.NEXT_PUBLIC_SITE_URL || (host ? `https://${host}` : 'https://esp-32-room-monitor.vercel.app');
      responseText = `🔗 *Aether Dashboard*\n[Open Web Interface](${siteUrl})`;
    } else if (text.startsWith('/status')) {
      const { data, error } = await supabase.from('room_readings').select('*').order('created_at', { ascending: false }).limit(1).single();
      if (error) {
        console.error('[Telegram Webhook] Supabase Error:', error);
        responseText = "⚠️ Error fetching latest reading from database.";
      } else if (data) {
        responseText = `📊 *LATEST STATUS*\n\n🌡 *Temp:* ${data.temperature.toFixed(1)}°C\n💧 *Hum:* ${data.humidity.toFixed(1)}%\n💡 *Light:* ${data.ldr_value} LUX\n🔋 *Battery:* ${data.battery_v.toFixed(2)}V\n🕒 *Time:* ${new Date(data.created_at).toLocaleString('en-SG', { timeZone: 'Asia/Singapore' })}`;
      }
    } else if (text.startsWith('/stats')) {
      const { data: sessions, error: sErr } = await supabase.from('device_sessions').select('duration');
      if (sErr) {
        console.error('[Telegram Webhook] Supabase Sessions Error:', sErr);
        responseText = "⚠️ Error fetching stats.";
      } else {
        const totalSec = sessions?.reduce((acc, s) => acc + s.duration, 0) || 0;
        const totalHrs = (totalSec / 3600).toFixed(1);
        responseText = `📈 *LIFETIME STATS*\n\n⏱ *Total Uptime:* ${totalHrs} hours\n🚀 *Boot Count:* ${sessions?.length || 0}\n📡 *Status:* Active`;
      }
    } else if (text.startsWith('/report')) {
      responseText = "📊 *Aether Report Center*\nSelect a timeframe for your environmental summary:";
      replyMarkup = {
        inline_keyboard: [
          [
            { text: "📅 24 Hours", callback_data: "report_day" },
            { text: "🗓 7 Days", callback_data: "report_week" }
          ],
          [
            { text: "📊 30 Days", callback_data: "report_month" },
            { text: "⏳ 1 Year", callback_data: "report_year" }
          ]
        ]
      };
    }

    if (responseText) {
      console.log(`[Telegram Webhook] Sending response to ${chatId}...`);
      const body: Record<string, unknown> = {
        chat_id: chatId,
        text: responseText,
        parse_mode: 'Markdown'
      };

      if (replyMarkup) {
        body.reply_markup = replyMarkup;
      }

      const telRes = await fetch(`https://api.telegram.org/bot${BOT_TOKEN}/sendMessage`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
      const telData = await telRes.json();
      console.log('[Telegram Webhook] Telegram Response:', JSON.stringify(telData));
    }

    return NextResponse.json({ ok: true });
  } catch (error: unknown) {
    console.error('[Telegram Webhook] Global Error:', error instanceof Error ? error.message : 'Unknown error');
    return NextResponse.json({ ok: false, error: error instanceof Error ? error.message : 'Unknown error' }, { status: 500 });
  }
}
