// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_truthtable.h
 *
 */

#ifndef NLID_TRUTHTABLE_H_
#define NLID_TRUTHTABLE_H_

#include "netlist/devices/nlid_system.h"
#include "netlist/nl_base.h"
#include "netlist/nl_setup.h"
#include "plib/ptypes.h"
#include "plib/putil.h"

#define USE_TT_ALTERNATIVE (0)

namespace netlist
{
namespace devices
{

	template<std::size_t m_NI, std::size_t m_NO>
	class NETLIB_NAME(truthtable_t) : public device_t
	{
	public:

		using type_t = typename plib::fast_type_for_bits<m_NO + m_NI>::type;

		static constexpr const std::size_t m_num_bits = m_NI;
		static constexpr const std::size_t m_size = (1 << (m_num_bits));
		static constexpr const type_t m_outmask = ((1 << m_NO) - 1);

		struct truthtable_t
		{
			truthtable_t()
			: m_timing_index{0}
			{}

			std::array<type_t, m_size> m_out_state;
			std::array<uint_least8_t, m_size * m_NO> m_timing_index;
			std::array<netlist_time, 16> m_timing_nt;
		};

		template <class C>
		nld_truthtable_t(C &owner, const pstring &name,
				const pstring &model,
				truthtable_t &ttp, const std::vector<pstring> &desc)
		: device_t(owner, name, model)
#if USE_TT_ALTERNATIVE
		, m_state(*this, "m_state", 0)
#endif
		, m_ign(*this, "m_ign", 0)
		, m_ttp(ttp)
		/* FIXME: the family should provide the names of the power-terminals! */
		, m_power_pins(*this)
		{
			init(desc);
		}

		void init(const std::vector<pstring> &desc);

		NETLIB_RESETI()
		{
			int active_outputs = 0;
			m_ign = 0;
#if USE_TT_ALTERNATIVE
			m_state = 0;
#endif
			for (std::size_t i = 0; i < m_NI; ++i)
			{
				m_I[i].activate();
#if USE_TT_ALTERNATIVE
				m_state |= (m_I[i]() << i);
#endif
			}
			for (auto &q : m_Q)
				if (q.has_net() && q.net().has_connections())
					active_outputs++;
			set_active_outputs(active_outputs);
		}

		// update is only called during startup here ...
		NETLIB_UPDATEI()
		{
#if USE_TT_ALTERNATIVE
			m_state = 0;
			for (std::size_t i = 0; i < m_NI; ++i)
			{
				m_state |= (m_I[i]() << i);
			}
#endif
			process<true>();
		}

#if USE_TT_ALTERNATIVE
		template <std::size_t N>
		void update_N() noexcept
		{
			m_state &= ~(1<<N);
			m_state |= (m_I[N]() << N);
			process<true>();
		}
#endif

		void inc_active() noexcept override
		{
			process<false>();
		}

		void dec_active() noexcept override
		{
			for (std::size_t i = 0; i< m_NI; i++)
				m_I[i].inactivate();
			m_ign = (1<<m_NI)-1;
		}

	protected:

	private:

		template<bool doOUT>
		void process() noexcept
		{
			netlist_time_ext mt(netlist_time_ext::zero());
			type_t nstate(0);
			type_t ign(m_ign);

			if (doOUT)
			{
#if !USE_TT_ALTERNATIVE
				for (auto I = m_I.begin(); ign != 0; ign >>= 1, ++I)
					if (ign & 1)
						I->activate();
				for (std::size_t i = 0; i < m_NI; i++)
					nstate |= (m_I[i]() << i);
#else
				nstate = m_state;
				for (std::size_t i = 0; ign != 0; ign >>= 1, ++i)
				{
					if (ign & 1)
					{
						nstate &= ~(1 << i);
						m_I[i].activate();
						nstate |= (m_I[i]() << i);
					}
				}
#endif
			}
			else
				for (std::size_t i = 0; i < m_NI; i++)
				{
					m_I[i].activate();
					nstate |= (m_I[i]() << i);
					mt = std::max(this->m_I[i].net().next_scheduled_time(), mt);
				}

			const type_t outstate(m_ttp.m_out_state[nstate]);
			type_t out(outstate & m_outmask);

			m_ign = outstate >> m_NO;

			const auto *t(&m_ttp.m_timing_index[nstate * m_NO]);

			if (doOUT)
				//for (std::size_t i = 0; i < m_NO; ++i)
				//	m_Q[i].push((out >> i) & 1, tim[t[i]]);
				this->push(out, t);
			else
			{
				const auto *tim = m_ttp.m_timing_nt.data();
				for (std::size_t i = 0; i < m_NO; ++i)
					m_Q[i].set_Q_time((out >> i) & 1, mt + tim[t[i]]);
			}

			ign = m_ign;
			for (auto I = m_I.begin(); ign != 0; ign >>= 1, ++I)
				if (ign & 1)
					I->inactivate();
#if USE_TT_ALTERNATIVE
			m_state = nstate;
#endif
		}

		template<typename T>
		void push(const T &v, const std::uint_least8_t * t)
		{
			if (m_NO >= 1) m_Q[0].push((v >> 0) & 1, m_ttp.m_timing_nt[t[0]]);
			if (m_NO >= 2) m_Q[1].push((v >> 1) & 1, m_ttp.m_timing_nt[t[1]]);
			if (m_NO >= 3) m_Q[2].push((v >> 2) & 1, m_ttp.m_timing_nt[t[2]]);
			if (m_NO >= 4) m_Q[3].push((v >> 3) & 1, m_ttp.m_timing_nt[t[3]]);
			if (m_NO >= 5) m_Q[4].push((v >> 4) & 1, m_ttp.m_timing_nt[t[4]]);
			if (m_NO >= 6) m_Q[5].push((v >> 5) & 1, m_ttp.m_timing_nt[t[5]]);
			if (m_NO >= 7) m_Q[6].push((v >> 6) & 1, m_ttp.m_timing_nt[t[6]]);
			if (m_NO >= 8) m_Q[7].push((v >> 7) & 1, m_ttp.m_timing_nt[t[7]]);
			for (std::size_t i = 8; i < m_NO; i++)
				m_Q[i].push((v >> i) & 1, m_ttp.m_timing_nt[t[i]]);
		}


		plib::uninitialised_array_t<logic_input_t, m_NI> m_I;
		plib::uninitialised_array_t<logic_output_t, m_NO> m_Q;

#if USE_TT_ALTERNATIVE
		state_var<type_t>   m_state;
#endif
		state_var<type_t>   m_ign;
		const truthtable_t  m_ttp;
		/* FIXME: the family should provide the names of the power-terminals! */
		nld_power_pins m_power_pins;
	};

} // namespace devices

namespace factory
{
	class truthtable_base_element_t : public factory::element_t
	{
	public:
		truthtable_base_element_t(const pstring &name,properties &&props);

		std::vector<pstring> m_desc;
		pstring m_family_name;
	};

	plib::unique_ptr<truthtable_base_element_t> truthtable_create(tt_desc &desc,
		properties &&props);

} // namespace factory
} // namespace netlist



#endif // NLID_TRUTHTABLE_H_
