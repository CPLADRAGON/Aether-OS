import { NextRequest, NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

const BOT_TOKEN = process.env.TELEGRAM_BOT_TOKEN;

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    console.log('[Telegram Webhook] Received body:', JSON.stringify(body));

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

    if (text.startsWith('/start')) {
      responseText = "👋 I'm Aether, your room monitor mascot! I've been successfully linked to your dashboard.\n\nTry /status to see live data.";
    } else if (text.startsWith('/url')) {
      const siteUrl = process.env.NEXT_PUBLIC_SITE_URL || 'https://aether-monitor.vercel.app';
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
      const { data, error } = await supabase.from('room_readings').select('temperature,humidity').order('created_at', { ascending: false }).limit(24);
      if (error) {
        console.error('[Telegram Webhook] Supabase Report Error:', error);
        responseText = "⚠️ Error generating report.";
      } else if (data && data.length > 0) {
        const avgT = data.reduce((acc, r) => acc + r.temperature, 0) / data.length;
        const avgH = data.reduce((acc, r) => acc + r.humidity, 0) / data.length;
        responseText = `📋 *24-READING REPORT*\n\n🌡 *Avg Temp:* ${avgT.toFixed(1)}°C\n💧 *Avg Hum:* ${avgH.toFixed(1)}%\n📊 *Samples:* ${data.length}`;
      } else {
        responseText = "❌ Insufficient data for report.";
      }
    }

    if (responseText) {
      console.log(`[Telegram Webhook] Sending response to ${chatId}...`);
      const telRes = await fetch(`https://api.telegram.org/bot${BOT_TOKEN}/sendMessage`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          chat_id: chatId,
          text: responseText,
          parse_mode: 'Markdown',
        }),
      });
      const telData = await telRes.json();
      console.log('[Telegram Webhook] Telegram Response:', JSON.stringify(telData));
    }

    return NextResponse.json({ ok: true });
  } catch (error: any) {
    console.error('[Telegram Webhook] Global Error:', error.message);
    return NextResponse.json({ ok: false, error: error.message }, { status: 500 });
  }
}
